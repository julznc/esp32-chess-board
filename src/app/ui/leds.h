
#pragma once

#include <led_strip/led_strip_encoder.h>

namespace ui::leds
{

#define LED_STRIP_NUMPIXELS         (8 * 8 * 2)

typedef enum {
    LED_OFF,     // led-off
    LED_RED_LOW,
    LED_RED,
    LED_ORANGE,
    LED_YELLOW,
    LED_GREEN,
  //LED_BLUE     // not okay
} led_color_et;


#define LIGHT_SQUARE(rank, file)     (((rank) ^ (file)) & 1)

bool init(void);
void update(void);
void clear(void);

void setColor(uint8_t u8_rank, uint8_t u8_file, uint8_t u8_red, uint8_t u8_green, uint8_t u8_blue);
void setColor(uint8_t u8_rank, uint8_t u8_file, led_color_et e_color);
void setColor(uint8_t /*square_et*/ e_square, led_color_et e_color);

//#define LED_TESTS

#ifdef LED_TESTS
void test(void);
void test2(void);
#endif

} // namespace ui::leds
