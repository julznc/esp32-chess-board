#ifndef __UI_LEDSTRIP_H__
#define __UI_LEDSTRIP_H__

#include <Adafruit_NeoPixel.h>

namespace ui::leds
{

#define LED_STRIP_NUMPIXELS         (8 * 8 * 2)


bool init(void);
void update(void);
void clear(void);

void setColor(uint16_t u16_offset, uint32_t u32_color);
void setColor(uint8_t u8_rank, uint8_t u8_file, uint8_t u8_red, uint8_t u8_green, uint8_t u8_blue);

void test(void);

} // namespace ui::leds

#endif // __UI_LEDSTRIP_H__
