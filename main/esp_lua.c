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
#include <string.h>

#include "esp_log.h"

#include "gpio_binding_app.h"
#include "sx_http_server.h"
#include "sx_lua_runtime.h"
#include "sx_wifi.h"

static const char *TAG = "esp_lua";

/* Hardcoded Wi-Fi STA credentials for this test build.
 *
 * Temporary: this exists only because POST /wifi_config (writing
 * credentials into NVS via sx_wifi_save_credentials()) is not built
 * yet. Once that endpoint exists, this define goes away and
 * connect_wifi_or_start_ap() below goes back to relying solely on
 * sx_wifi_load_credentials(). Until then, change these two lines
 * whenever the test network changes -- nothing else in this file
 * needs to change.
 *
 * WIFI_PASS may be "" for an open network. */
#define WIFI_SSID "Log Terminal "
#define WIFI_PASS "locdeptrai"

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

/* Wi-Fi policy lives here, not in sx_wifi: this app decides that a
 * saved-credential STA attempt is tried first, then the hardcoded
 * WIFI_SSID/WIFI_PASS test credentials above, and a fallback AP is
 * used only if neither works. sx_wifi itself has no opinion on any
 * of this -- it only exposes start_sta() / start_ap() /
 * load_credentials() as mechanism. */
static void connect_wifi_or_start_ap(void)
{
    if (!sx_wifi_init()) {
        ESP_LOGE(TAG, "sx_wifi_init failed, cannot bring up networking");
        return;
    }

    char ssid[SX_WIFI_SSID_MAX_LEN] = { 0 };
    char password[SX_WIFI_PASSWORD_MAX_LEN] = { 0 };

    if (sx_wifi_load_credentials(ssid, sizeof(ssid), password, sizeof(password))) {
        ESP_LOGI(TAG, "Found saved credentials for SSID '%s', attempting STA connect", ssid);
        if (sx_wifi_start_sta(ssid, password)) {
            ESP_LOGI(TAG, "Wi-Fi up in STA mode (saved credentials)");
            return;
        }
        ESP_LOGW(TAG, "Saved-credential STA connect failed, trying hardcoded test credentials");
    } else {
        ESP_LOGI(TAG, "No saved Wi-Fi credentials, trying hardcoded test credentials");
    }

    ESP_LOGI(TAG, "Attempting STA connect with hardcoded WIFI_SSID '%s'", WIFI_SSID);
    if (sx_wifi_start_sta(WIFI_SSID, WIFI_PASS)) {
        ESP_LOGI(TAG, "Wi-Fi up in STA mode (hardcoded test credentials)");
        return;
    }
    ESP_LOGW(TAG, "Hardcoded STA connect failed, falling back to AP mode");

    char ap_ssid[SX_WIFI_SSID_MAX_LEN] = { 0 };
    if (!sx_wifi_build_default_ap_ssid(ap_ssid, sizeof(ap_ssid))) {
        strlcpy(ap_ssid, "ESP32-LUA-SETUP", sizeof(ap_ssid));
    }

    if (sx_wifi_start_ap(ap_ssid, NULL)) {
        ESP_LOGW(TAG, "Wi-Fi up in fallback AP mode (SSID '%s', open network)", ap_ssid);
    } else {
        ESP_LOGE(TAG, "Failed to start fallback AP -- no network interface available");
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting esp32_lua on ESP32-S3-N16R8");

    connect_wifi_or_start_ap();

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