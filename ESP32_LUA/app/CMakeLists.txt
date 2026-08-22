# app/user/gpio_binding_app
#
# Top layer of the component -> service -> app chain. The only module
# that depends on both sx_gpio (peripherals/gpio) and sx_lua_runtime
# (services/lua_runtime) at once. Registers Lua-callable gpio.write/
# gpio.read backed by a fixed, allow-listed set of pins.

idf_component_register(
    SRCS "gpio_binding_app.c"
    INCLUDE_DIRS "."
    REQUIRES sx_gpio lua_runtime driver
)