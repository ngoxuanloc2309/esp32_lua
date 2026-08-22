#ifndef GPIO_BINDING_APP_H
#define GPIO_BINDING_APP_H
#ifdef __cplusplus
extern "C" {
#endif

#include "sx_lua_runtime.h"

/* Top layer of the component -> service -> app chain.
 *
 * Registers a fixed, allow-listed set of GPIO pins as a Lua-callable
 * "gpio" table (gpio.write(pin, level), gpio.read(pin)) on the given
 * runtime. Must be called once, after sx_lua_runtime_init() and
 * before running any script that uses gpio.*. */
void gpio_binding_app_register(sx_lua_runtime_t *runtime);

#ifdef __cplusplus
}
#endif
#endif /* GPIO_BINDING_APP_H */
