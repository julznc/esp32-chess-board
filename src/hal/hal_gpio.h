
#pragma once

#include <driver/gpio.h>

#include "hal_gpio_cfg.h"

#ifdef __cplusplus
extern "C" {
#endif

void hal_gpio_init(void);

#define PIN_READ(pin)           gpio_get_level(PIN_##pin)
#define PIN_WRITE(pin, val)     gpio_set_level(PIN_##pin, val)
#define PIN_HIGH(pin)           gpio_set_level(PIN_##pin, 1)
#define PIN_LOW(pin)            gpio_set_level(PIN_##pin, 0)


#define LED_ON()                PIN_HIGH(LED)
#define LED_OFF()               PIN_LOW(LED)


#ifdef __cplusplus
}
#endif
