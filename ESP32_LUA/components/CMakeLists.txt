# peripherals/gpio
#
# Lowest layer of the component -> service -> app chain. Wraps the
# ESP-IDF GPIO driver behind the sx_gpio_ops_t interface (sx_gpio.h) so
# that nothing above this layer depends on driver/gpio.h directly.
# Depends only on ESP-IDF's "driver" component; must never depend on
# "lua_component" or anything under services/ or app/.

idf_component_register(
    SRCS "sx_gpio.c"
    INCLUDE_DIRS "."
    REQUIRES driver
)