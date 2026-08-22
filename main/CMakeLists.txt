idf_component_register(SRCS "esp_lua.c"
                    INCLUDE_DIRS "."
                    REQUIRES "lua_runtime" "gpio_binding_app" "driver")