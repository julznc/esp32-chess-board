
#pragma once

// built-in led
#define PIN_LED             (GPIO_NUM_15)
// board leds
#define PIN_LED_STRIP       (GPIO_NUM_21)

// oled i2c
#define PIN_OLED_SDA        (GPIO_NUM_9)
#define PIN_OLED_SCL        (GPIO_NUM_10)

// battery monitor
#define PIN_BATT_ADC        (GPIO_NUM_16)    // battery voltage monitor (via 3k3 & 5k6 divider)

// user push buttons
#define PIN_BTN_1           (GPIO_NUM_11)    // SW3 (upper left)
#define PIN_BTN_2           (GPIO_NUM_12)    // SW2 (lower left)
#define PIN_BTN_3           (GPIO_NUM_14)    // SW4 (lower right)

// RFIDs SPI
#define PIN_RFID_MISO       (GPIO_NUM_33)
#define PIN_RFID_MOSI       (GPIO_NUM_34)
#define PIN_RFID_SCK        (GPIO_NUM_35)
#define PIN_RFID_CS         (GPIO_NUM_1)     // active high (inverted)
#define PIN_RFID_RST        (GPIO_NUM_5)     // active low

// multiplexed via HC238
#define PIN_RFID_RST_A      (GPIO_NUM_8)
#define PIN_RFID_RST_B      (GPIO_NUM_7)
#define PIN_RFID_RST_C      (GPIO_NUM_6)

// multiplexed via HC138
#define PIN_RFID_SS_A       (GPIO_NUM_4)
#define PIN_RFID_SS_B       (GPIO_NUM_3)
#define PIN_RFID_SS_C       (GPIO_NUM_2)
