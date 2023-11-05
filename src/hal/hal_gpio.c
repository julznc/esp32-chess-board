
#include "hal_gpio.h"


void hal_gpio_init(void)
{
    gpio_set_direction(PIN_LED, GPIO_MODE_OUTPUT);
}
