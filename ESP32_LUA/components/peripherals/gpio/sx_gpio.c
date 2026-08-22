#include "sx_gpio.h"

#include "driver/gpio.h"

/* Note: this component does not call gpio_config() itself. Pin
 * direction/pull configuration is a one-time setup concern owned by
 * whoever wires up an sx_gpio_t instance (currently the app layer,
 * see gpio_binding_app.c), not by every read/write call. This keeps
 * sx_gpio_write()/sx_gpio_read() cheap and side-effect-free beyond the
 * actual I/O operation. */

static int _sx_gpio_write(sx_gpio_t *gpio, SX_GPIO_VALUE value)
{
    sx_gpio_pin_t *pin = (sx_gpio_pin_t *)gpio->pDriver;
    esp_err_t err = gpio_set_level((gpio_num_t)pin->pin,
                                    value == SX_GPIO_HIGH ? 1 : 0);
    return err == ESP_OK ? 0 : -1;
}

static int _sx_gpio_read(sx_gpio_t *gpio, SX_GPIO_VALUE *value)
{
    sx_gpio_pin_t *pin = (sx_gpio_pin_t *)gpio->pDriver;
    int level = gpio_get_level((gpio_num_t)pin->pin);
    *value = (level != 0) ? SX_GPIO_HIGH : SX_GPIO_LOW;
    return 0;
}

static int _sx_gpio_toggle(sx_gpio_t *gpio)
{
    SX_GPIO_VALUE current;
    int ret = _sx_gpio_read(gpio, &current);
    if (ret != 0) {
        return ret;
    }
    SX_GPIO_VALUE next = (current == SX_GPIO_LOW) ? SX_GPIO_HIGH : SX_GPIO_LOW;
    return _sx_gpio_write(gpio, next);
}

sx_gpio_ops_t sx_gpio_ops = {
    .write  = _sx_gpio_write,
    .read   = _sx_gpio_read,
    .toggle = _sx_gpio_toggle,
};