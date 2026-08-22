#ifndef SX_GPIO_H
#define SX_GPIO_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Lowest layer of the component -> service -> app dependency chain.
 * This header must never include or reference anything from the
 * service or app layers (in particular: nothing Lua-related). It only
 * knows about digital I/O; callers decide what that I/O is used for. */

typedef enum SX_GPIO_VALUE {
    SX_GPIO_LOW = 0,
    SX_GPIO_HIGH = 1
} SX_GPIO_VALUE;

typedef struct sx_gpio sx_gpio_t;

/* Platform-specific pin identity. On ESP-IDF a GPIO is identified by a
 * single number (gpio_num_t), so "port" is unused here and kept only
 * for structural symmetry with STM32 targets (where a pin is a
 * GPIO_TypeDef* port + pin-number pair) -- this keeps sx_gpio_pin_t's
 * shape reusable across the ESP32 and STM32 implementations. */
typedef struct {
    void    *port;   /* unused on ESP-IDF; reserved for STM32 GPIO_TypeDef* */
    uint32_t pin;     /* ESP-IDF gpio_num_t value */
} sx_gpio_pin_t;

typedef struct sx_gpio_ops {
    int (*write)(sx_gpio_t *gpio, SX_GPIO_VALUE value);
    int (*read)(sx_gpio_t *gpio, SX_GPIO_VALUE *value);
    int (*toggle)(sx_gpio_t *gpio);
} sx_gpio_ops_t;

typedef struct sx_gpio {
    sx_gpio_ops_t *ops;
    void *pDriver;       /* points at an sx_gpio_pin_t for this instance */
    SX_GPIO_VALUE value;  /* last known value, tracked on successful write/toggle */
} sx_gpio_t;

static inline void sx_gpio_init(sx_gpio_t *gpio, sx_gpio_ops_t *ops, void *pDriver)
{
    gpio->ops = ops;
    gpio->pDriver = pDriver;
    gpio->value = SX_GPIO_LOW;
}

static inline int sx_gpio_write(sx_gpio_t *gpio, SX_GPIO_VALUE value)
{
    if (gpio->ops && gpio->ops->write) {
        int ret = gpio->ops->write(gpio, value);
        if (ret == 0) {
            gpio->value = value;
        }
        return ret;
    }
    return -1;
}

static inline int sx_gpio_read(sx_gpio_t *gpio, SX_GPIO_VALUE *value)
{
    if (gpio->ops && gpio->ops->read) {
        return gpio->ops->read(gpio, value);
    }
    return -1;
}

static inline int sx_gpio_toggle(sx_gpio_t *gpio)
{
    if (gpio->ops && gpio->ops->toggle) {
        int ret = gpio->ops->toggle(gpio);
        if (ret == 0) {
            gpio->value = (gpio->value == SX_GPIO_LOW) ? SX_GPIO_HIGH : SX_GPIO_LOW;
        }
        return ret;
    }
    return -1;
}

/* ESP-IDF implementation of the ops table, defined in sx_gpio.c. A
 * future STM32 port provides its own translation unit with the same
 * symbol name, backed by HAL_GPIO_* calls instead of gpio_set_level/
 * gpio_get_level -- callers above this layer never need to change. */
extern sx_gpio_ops_t sx_gpio_ops;

#ifdef __cplusplus
}
#endif
#endif /* SX_GPIO_H */