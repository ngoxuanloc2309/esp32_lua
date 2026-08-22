#include "sx_http_server.h"

#include <string.h>

#include "esp_http_server.h"
#include "esp_log.h"

static const char *TAG = "sx_http_server";

/* Requests larger than this are rejected before the callback runs, to
 * bound how much heap a single request can force us to allocate for
 * the body buffer. Generated Lua scripts are expected to be small
 * (a handful of KB at most); raise this if a later stage needs to
 * upload larger scripts. */
#define SX_HTTP_SERVER_MAX_BODY_LEN 8192

/* httpd_uri_t's user_ctx is the only way to reach back into our own
 * sx_http_server_t from the handler, since esp_http_server calls
 * handlers with no other caller-supplied state. */
static esp_err_t run_lua_post_handler(httpd_req_t *req)
{
    sx_http_server_t *server = (sx_http_server_t *)req->user_ctx;

    if (req->content_len <= 0 || req->content_len > SX_HTTP_SERVER_MAX_BODY_LEN) {
        ESP_LOGW(TAG, "POST /run_lua rejected: content_len=%d (max %d)",
                 req->content_len, SX_HTTP_SERVER_MAX_BODY_LEN);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "body missing or too large");
        return ESP_FAIL;
    }

    /* +1 for the NUL terminator the run callback is documented to
     * receive; body_len passed to the callback excludes it. */
    char *body = malloc(req->content_len + 1);
    if (body == NULL) {
        ESP_LOGE(TAG, "POST /run_lua: failed to allocate %d-byte body buffer",
                 req->content_len + 1);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "out of memory");
        return ESP_FAIL;
    }

    int received = 0;
    while (received < req->content_len) {
        int ret = httpd_req_recv(req, body + received, req->content_len - received);
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                httpd_resp_send_408(req);
            }
            free(body);
            return ESP_FAIL;
        }
        received += ret;
    }
    body[received] = '\0';

    bool ok = false;
    if (server != NULL && server->run_cb != NULL) {
        ok = server->run_cb(body, (size_t)received, server->run_cb_ctx);
    } else {
        ESP_LOGW(TAG, "POST /run_lua received but no run callback is registered");
    }

    free(body);

    if (ok) {
        httpd_resp_set_status(req, "200 OK");
        httpd_resp_sendstr(req, "OK");
    } else {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_sendstr(req, "ERROR");
    }
    return ESP_OK;
}

void sx_http_server_init(sx_http_server_t *server)
{
    memset(server, 0, sizeof(*server));
}

void sx_http_server_set_run_callback(sx_http_server_t *server,
                                      sx_http_server_run_cb cb,
                                      void *user_ctx)
{
    server->run_cb = cb;
    server->run_cb_ctx = user_ctx;
}

bool sx_http_server_start(sx_http_server_t *server)
{
    if (server->httpd_handle != NULL) {
        ESP_LOGW(TAG, "sx_http_server_start() called but server is already running");
        return true;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    httpd_handle_t handle = NULL;
    esp_err_t err = httpd_start(&handle, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed: %s", esp_err_to_name(err));
        return false;
    }

    httpd_uri_t run_lua_uri = {
        .uri = "/run_lua",
        .method = HTTP_POST,
        .handler = run_lua_post_handler,
        .user_ctx = server,
    };

    err = httpd_register_uri_handler(handle, &run_lua_uri);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to register POST /run_lua: %s", esp_err_to_name(err));
        httpd_stop(handle);
        return false;
    }

    server->httpd_handle = (void *)handle;
    ESP_LOGI(TAG, "HTTP server started, POST /run_lua ready on port %d", config.server_port);
    return true;
}

void sx_http_server_stop(sx_http_server_t *server)
{
    if (server->httpd_handle == NULL) {
        return;
    }
    httpd_stop((httpd_handle_t)server->httpd_handle);
    server->httpd_handle = NULL;
}