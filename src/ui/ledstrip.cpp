
#include "globals.h"
#include "ledstrip.h"


namespace ui::leds
{

Adafruit_NeoPixel pixels(LED_STRIP_NUMPIXELS, LED_STRIP_PIN, NEO_GRB + NEO_KHZ800);


bool init(void)
{
    pixels.begin();

    clear();
    update();

    return true;
}

void update(void)
{
    pixels.show();
}

void clear(void)
{
    pixels.fill(pixels.Color(0, 0, 0), 0, LED_STRIP_NUMPIXELS);
}

void setColor(uint16_t u16_offset, uint32_t u32_color)
{
    pixels.setPixelColor(u16_offset, u32_color);
}

void setColor(uint8_t u8_rank, uint8_t u8_file, uint8_t u8_red, uint8_t u8_green, uint8_t u8_blue)
{
    uint16_t u16_mid     = (uint16_t)(u8_file >> 2) << 6;
    uint16_t u16_offset  = u16_mid + ((u8_rank) << 3);

    u8_file &= 0x03; // %4
    pixels.setPixelColor(u16_offset + u8_file,     pixels.Color(u8_red, u8_green, u8_blue));
    pixels.setPixelColor(u16_offset + 7 - u8_file, pixels.Color(u8_red, u8_green, u8_blue));
}

void test(void)
{
    static const uint16_t count = 32;
    static uint32_t prev_ms = 0;
    static uint16_t prev_led = count - 1;

    if (millis() - prev_ms > 100)
    {
        prev_ms = millis();

        pixels.setPixelColor(prev_led, pixels.Color(0, 0, 0));
        prev_led = (prev_led + 1) % count;
        pixels.setPixelColor(prev_led, pixels.Color(rand(), rand(), rand()));
        pixels.show();
    }
}

} // namespace ui::leds
