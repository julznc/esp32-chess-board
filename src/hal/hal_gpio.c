
#include "hal_gpio.h"


void hal_gpio_init(void)
{
    gpio_config_t io_conf = {0, };

    gpio_set_direction(PIN_LED, GPIO_MODE_OUTPUT);

    // configure push buttons
    io_conf.pin_bit_mask = (1 << PIN_BTN_1) | (1 << PIN_BTN_2) | (1 << PIN_BTN_3);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    ESP_ERROR_CHECK( gpio_config(&io_conf) );

    // configure rfids i/o
    io_conf.pin_bit_mask  = (1 << PIN_RFID_RST);
    io_conf.pin_bit_mask |= (1 << PIN_RFID_RST_A) | (1 << PIN_RFID_RST_B) | (1 << PIN_RFID_RST_C);
    io_conf.pin_bit_mask |= (1 << PIN_RFID_CS_A)  | (1 << PIN_RFID_CS_B)  | (1 << PIN_RFID_CS_C);
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    ESP_ERROR_CHECK( gpio_config(&io_conf) );
}
