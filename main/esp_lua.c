/*
 * esp_lua.c
 *
 * Orchestrator only. This file must not call the Lua C API directly
 * and must not touch any ESP-IDF driver header -- all of that lives
 * behind sx_lua_runtime (services/lua_runtime) and gpio_binding_app
 * (app/user/gpio_binding_app). main/ just sequences the three
 * high-level calls needed to bring the runtime up, wire in the
 * bindings this build needs, and run a script.
 *
 * Target board: ESP32-S3-N16R8, ESP-IDF / FreeRTOS.
 */

#include <stdbool.h>

#include "esp_log.h"

#include "gpio_binding_app.h"
#include "sx_lua_runtime.h"

static const char *TAG = "esp_lua";

/* Exercises the gpio binding registered by gpio_binding_app_register().
 * Still a fixed, hardcoded script at this stage -- this is the same
 * luaL_dostring() entry point (now behind sx_lua_runtime_run_string())
 * that a later stage will feed with Blockly-generated script text
 * received over HTTP instead. */
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

void app_main(void)
{
    ESP_LOGI(TAG, "Starting GPIO binding test on ESP32-S3-N16R8");

    sx_lua_runtime_t runtime;
    if (!sx_lua_runtime_init(&runtime)) {
        ESP_LOGE(TAG, "Failed to initialize Lua runtime");
        return;
    }

    gpio_binding_app_register(&runtime);

    bool ok = sx_lua_runtime_run_string(&runtime, kGpioTestScript);

    sx_lua_runtime_deinit(&runtime);

    if (ok) {
        ESP_LOGI(TAG, "GPIO binding test PASSED");
    } else {
        ESP_LOGE(TAG, "GPIO binding test FAILED");
    }
}