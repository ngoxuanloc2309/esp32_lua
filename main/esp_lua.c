/*
 * esp_lua.c
 *
 * Orchestrator only. This file must not call the Lua C API directly
 * and must not touch any ESP-IDF driver header -- all of that lives
 * behind sx_wifi (services/wifi), sx_lua_runtime (services/lua_runtime),
 * sx_http_server (services/http_server) and gpio_binding_app
 * (app/user/gpio_binding_app). main/ just sequences the high-level
 * calls needed to bring networking up, bring the Lua runtime up, wire
 * in the bindings this build needs, and start accepting scripts over
 * HTTP.
 *
 * Target board: ESP32-S3-N16R8, ESP-IDF / FreeRTOS.
 */

#include <stdbool.h>
#include <stddef.h>

#include "esp_log.h"

#include "gpio_binding_app.h"
#include "sx_http_server.h"
#include "sx_lua_runtime.h"
#include "sx_wifi.h"

static const char *TAG = "esp_lua";

/* The Lua runtime lives for the lifetime of app_main() (it never
 * returns on a normal boot), so it's held here as a static rather
 * than a stack variable, and its address is what gets passed as the
 * HTTP server's run_cb user_ctx below. */
static sx_lua_runtime_t s_runtime;

/* Exercised once at boot before the HTTP server takes over, purely to
 * confirm the gpio binding is wired correctly on this build -- the
 * same sx_lua_runtime_run_string() entry point is what the HTTP
 * callback below now feeds with request bodies instead. */
static const char *kGpioTestScript =
    "gpio.write(38, 1)\n"
    "local level_on = gpio.read(38)\n"
    "print(\"GPIO38 after write(1): \" .. level_on)\n"
    "\n"
    "gpio.write(38, 0)\n"
    "local level_off = gpio.read(38)\n"
    "print(\"GPIO38 after write(0): \" .. level_off)\n"
    "\n"
    "return level_on, level_off\n";

/* sx_http_server_run_cb: invoked on the HTTP server's own task for
 * every POST /run_lua request. user_ctx is the sx_lua_runtime_t*
 * registered below. Body is already NUL-terminated by sx_http_server.
 *
 * sx_lua_runtime_run_string() is not reentrant against itself (single
 * lua_State), which is fine here because esp_http_server processes
 * requests on a single task by default -- if that ever changes,
 * this callback would need its own mutex around the runtime. */
static bool http_run_lua_cb(const char *body, size_t body_len, void *user_ctx)
{
    (void)body_len;
    sx_lua_runtime_t *runtime = (sx_lua_runtime_t *)user_ctx;
    return sx_lua_runtime_run_string(runtime, body);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting esp32_lua on ESP32-S3-N16R8");

    bool sta_connected = sx_wifi_start_auto();
    if (sta_connected) {
        ESP_LOGI(TAG, "Wi-Fi up in STA mode");
    } else {
        ESP_LOGW(TAG, "Wi-Fi up in fallback AP mode (no STA connection)");
    }

    if (!sx_lua_runtime_init(&s_runtime)) {
        ESP_LOGE(TAG, "Failed to initialize Lua runtime");
        return;
    }

    gpio_binding_app_register(&s_runtime);

    ESP_LOGI(TAG, "Running boot-time GPIO binding self-test...");
    bool ok = sx_lua_runtime_run_string(&s_runtime, kGpioTestScript);
    if (ok) {
        ESP_LOGI(TAG, "GPIO binding self-test PASSED");
    } else {
        ESP_LOGE(TAG, "GPIO binding self-test FAILED (continuing anyway)");
    }

    static sx_http_server_t http_server;
    sx_http_server_init(&http_server);
    sx_http_server_set_run_callback(&http_server, http_run_lua_cb, &s_runtime);

    if (!sx_http_server_start(&http_server)) {
        ESP_LOGE(TAG, "Failed to start HTTP server -- scripts cannot be received over the network");
        return;
    }

    ESP_LOGI(TAG, "Ready: POST Lua scripts to http://<device-ip>/run_lua");

    /* app_main() returning is not an error in ESP-IDF (it just tears
     * down the main task), but this firmware has nothing left to do
     * on that task -- the HTTP server and Wi-Fi driver run their own
     * tasks. Nothing above needs cleanup on a normal, indefinite run,
     * so this simply falls through and returns. */
}