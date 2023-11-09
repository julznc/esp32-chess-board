
#pragma once

// built-in led
#define PIN_LED             (GPIO_NUM_15)

// oled i2c
#define PIN_OLED_SDA        (GPIO_NUM_9)
#define PIN_OLED_SCL        (GPIO_NUM_10)

// battery monitor
#define PIN_BATT_ADC        (GPIO_NUM_16)    // battery voltage monitor (via 3k3 & 5k6 divider)

// user push buttons
#define PIN_BTN_1           (GPIO_NUM_11)    // SW3 (upper left)
#define PIN_BTN_2           (GPIO_NUM_12)    // SW2 (lower left)
#define PIN_BTN_3           (GPIO_NUM_14)    // SW4 (lower right)
