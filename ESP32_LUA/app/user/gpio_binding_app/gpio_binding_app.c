#include "gpio_binding_app.h"

#include "driver/gpio.h"
#include "lauxlib.h"
#include "sx_gpio.h"

/* Pins this binding exposes to Lua. Extend this array (and
 * s_pin_backing/s_gpio_instances below) to add more DI/DO channels --
 * a pin not listed here is rejected by find_pin_or_error() rather
 * than silently forwarded to the driver. */
#define TEST_GPIO_PIN 38

static sx_gpio_pin_t s_pin_backing[] = {
    { .port = NULL, .pin = TEST_GPIO_PIN },
};

static sx_gpio_t s_gpio_instances[sizeof(s_pin_backing) / sizeof(s_pin_backing[0])];

#define NUM_BOUND_PINS (sizeof(s_gpio_instances) / sizeof(s_gpio_instances[0]))

/* One-time hardware setup for every pin this binding exposes. Kept
 * separate from sx_gpio (which only reads/writes level, not
 * direction) and from the Lua-facing functions below (which must stay
 * cheap and side-effect-free beyond the actual I/O call). */
static void configure_pin_hardware(void)
{
    uint64_t pin_mask = 0;
    for (size_t i = 0; i < NUM_BOUND_PINS; i++) {
        pin_mask |= (1ULL << s_pin_backing[i].pin);
    }

    gpio_config_t io_conf = {
        .pin_bit_mask = pin_mask,
        .mode = GPIO_MODE_INPUT_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    for (size_t i = 0; i < NUM_BOUND_PINS; i++) {
        sx_gpio_init(&s_gpio_instances[i], &sx_gpio_ops, &s_pin_backing[i]);
    }
}

/* Finds the sx_gpio_t bound to the requested pin number, or raises a
 * Lua error and does not return if the pin isn't in s_pin_backing.
 * This is the allow-list check: a script cannot address a pin this
 * module hasn't explicitly configured. */
static sx_gpio_t *find_pin_or_error(lua_State *L, lua_Integer pin)
{
    for (size_t i = 0; i < NUM_BOUND_PINS; i++) {
        if (s_pin_backing[i].pin == (uint32_t)pin) {
            return &s_gpio_instances[i];
        }
    }
    luaL_error(L, "gpio: pin %d is not bound by this app build", (int)pin);
    return NULL; /* unreachable: luaL_error() longjmps out */
}

/* Lua-callable: gpio.write(pin, level)
 * level accepts an integer (0/non-zero) or a boolean. */
static int l_gpio_write(lua_State *L)
{
    lua_Integer pin = luaL_checkinteger(L, 1);
    SX_GPIO_VALUE level;

    if (lua_isboolean(L, 2)) {
        level = lua_toboolean(L, 2) ? SX_GPIO_HIGH : SX_GPIO_LOW;
    } else {
        level = (luaL_checkinteger(L, 2) != 0) ? SX_GPIO_HIGH : SX_GPIO_LOW;
    }

    sx_gpio_t *gpio = find_pin_or_error(L, pin);

    if (sx_gpio_write(gpio, level) != 0) {
        return luaL_error(L, "gpio.write: failed to set pin %d", (int)pin);
    }
    return 0;
}

/* Lua-callable: gpio.read(pin) -> integer 0 or 1 */
static int l_gpio_read(lua_State *L)
{
    lua_Integer pin = luaL_checkinteger(L, 1);
    sx_gpio_t *gpio = find_pin_or_error(L, pin);

    SX_GPIO_VALUE level;
    if (sx_gpio_read(gpio, &level) != 0) {
        return luaL_error(L, "gpio.read: failed to read pin %d", (int)pin);
    }

    lua_pushinteger(L, level == SX_GPIO_HIGH ? 1 : 0);
    return 1;
}

/* Registers the "gpio" table directly via the Lua C API (rather than
 * sx_lua_runtime_register_function(), which only supports flat
 * globals) since gpio.write/gpio.read need to live inside a "gpio"
 * namespace table, not as bare globals. */
static void register_gpio_table(lua_State *L)
{
    static const luaL_Reg gpio_functions[] = {
        {"write", l_gpio_write},
        {"read",  l_gpio_read},
        {NULL, NULL}
    };
    luaL_newlib(L, gpio_functions);
    lua_setglobal(L, "gpio");
}

void gpio_binding_app_register(sx_lua_runtime_t *runtime)
{
    configure_pin_hardware();
    register_gpio_table(runtime->L);
}