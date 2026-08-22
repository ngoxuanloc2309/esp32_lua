#ifndef SX_HTTP_SERVER_H
#define SX_HTTP_SERVER_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>

/* Middle layer of the component -> service -> app chain.
 *
 * This service owns the ESP-IDF HTTP server (esp_http_server)
 * lifecycle and exposes a single generic endpoint,
 * POST /run_lua, that accepts a raw request body and hands it off to
 * a caller-supplied callback. It deliberately knows nothing about
 * Lua, GPIO, or any other peripheral -- the app layer registers a
 * callback that decides what "running" the received text actually
 * means (see app/user/http_lua_bridge). This is what lets this
 * service be reused unmodified across different apps/boards: what
 * happens to a POSTed body is an app-layer decision, not something
 * this service hardcodes.
 *
 * Later stages (Blockly editor, generated script upload) will POST
 * their generated Lua source to this same /run_lua endpoint without
 * requiring any change here. */

typedef struct sx_http_server sx_http_server_t;

/* Callback invoked once per POST /run_lua request, with the raw
 * request body as a NUL-terminated string (body_len excludes the
 * NUL). Must return true if the body was accepted and handled
 * successfully; the server reports this back to the HTTP client as
 * plain text ("OK" / "ERROR") with a 200 / 500 status respectively.
 * Runs on the HTTP server's own task -- keep it non-blocking and
 * thread-safe with respect to whatever state it touches. */
typedef bool (*sx_http_server_run_cb)(const char *body, size_t body_len,
                                       void *user_ctx);

struct sx_http_server {
    void *httpd_handle;              /* opaque httpd_handle_t, NULL if not started */
    sx_http_server_run_cb run_cb;    /* callback registered via sx_http_server_set_run_callback() */
    void *run_cb_ctx;                /* opaque context passed back to run_cb unchanged */
};

/* Zero-initializes *server. Does not start the HTTP server yet --
 * call sx_http_server_start() once a run callback has been
 * registered. Safe to call on stack- or static-allocated storage. */
void sx_http_server_init(sx_http_server_t *server);

/* Registers the callback invoked for each POST /run_lua request.
 * Must be called before sx_http_server_start(). user_ctx is passed
 * back to the callback unchanged on every call (e.g. a pointer to the
 * sx_lua_runtime_t the app layer wants requests run against). */
void sx_http_server_set_run_callback(sx_http_server_t *server,
                                      sx_http_server_run_cb cb,
                                      void *user_ctx);

/* Starts the HTTP server and registers the POST /run_lua route.
 * Requires an active network interface (Wi-Fi/Ethernet already
 * connected) -- this service does not bring up networking itself.
 * Returns true on success. */
bool sx_http_server_start(sx_http_server_t *server);

/* Stops the HTTP server and releases its resources. Safe to call on a
 * server that failed sx_http_server_start() or was never started
 * (zero-initialized sx_http_server_t). */
void sx_http_server_stop(sx_http_server_t *server);

#ifdef __cplusplus
}
#endif
#endif /* SX_HTTP_SERVER_H */