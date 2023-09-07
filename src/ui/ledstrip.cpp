
#include "globals.h"
#include "buttons.h"
#include "ledstrip.h"


namespace ui::leds
{

Adafruit_NeoPixel pixels(LED_STRIP_NUMPIXELS, LED_STRIP_PIN, NEO_GRB + NEO_KHZ800);


#ifdef LED_TESTS

//static uint8_t au8_test_rgb[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static uint8_t au8_test_rgb[6] = {0xFF, 0xFF, 0x00, 0xFF, 0xFF, 0x00}; // no blue
static uint8_t u8_test_idx = 0;

#endif

bool init(void)
{
    pixels.begin();
    pixels.setBrightness(255);

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

void setColor(uint8_t u8_rank, uint8_t u8_file, uint8_t u8_red, uint8_t u8_green, uint8_t u8_blue)
{
    uint16_t u16_mid     = (uint16_t)(u8_file >> 2) << 6;
    uint16_t u16_offset  = u16_mid + ((u8_rank) << 3);

    u8_file &= 0x03; // %4
    pixels.setPixelColor(u16_offset + u8_file,     pixels.Color(u8_red, u8_green, u8_blue));
    pixels.setPixelColor(u16_offset + 7 - u8_file, pixels.Color(u8_red, u8_green, u8_blue));
}

void setColor(uint8_t u8_rank, uint8_t u8_file, led_color_et e_color)
{
#ifdef LED_TESTS
    /*
      red_low:  06 00 00  -  10 00 00
      red:      50 00 00  -  ff 00 00
      orange:   30 0f 00  -  50 df 00
      yellow:   10 0f 00  -  20 ff 00
      green:    04 0f 00  -  00 ff 00
    */
    if (LIGHT_SQUARE(u8_rank, u8_file))
    {
        setColor(u8_rank, u8_file, au8_test_rgb[0], au8_test_rgb[1], au8_test_rgb[2]);
    }
    else
    {
        setColor(u8_rank, u8_file, au8_test_rgb[3], au8_test_rgb[4], au8_test_rgb[5]);
    }
#else

    static const uint8_t K_RGB[][6] = {
        // light squares  , dark squares
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, // off
        { 0x06, 0x00, 0x00, 0x10, 0x00, 0x00 }, // red_low
        { 0x50, 0x00, 0x00, 0xff, 0x00, 0x00 }, // red
        { 0x30, 0x0f, 0x00, 0x50, 0xdf, 0x00 }, // orange
        { 0x10, 0x0f, 0x00, 0x20, 0xff, 0x00 }, // yellow
        { 0x04, 0x0f, 0x00, 0x00, 0xff, 0x00 }, // green
    };

    uint8_t u8_idx = 0;
    uint8_t u8_offset = 0;

    if ((e_color >= LED_RED_LOW) && (e_color <= LED_GREEN)) {
        u8_idx = e_color;
    }

    if (!LIGHT_SQUARE(u8_rank, u8_file)) {
        u8_offset = 3; // dark square
    }

    setColor(u8_rank, u8_file,
            K_RGB[u8_idx][u8_offset],
            K_RGB[u8_idx][u8_offset+1],
            K_RGB[u8_idx][u8_offset+2]);
#endif
}

void setColor(uint8_t /*square_et*/ e_square, led_color_et e_color)
{
    uint8_t u8_rank = 7 - (e_square >> 4);
    uint8_t u8_file = (e_square & 0xF);

    setColor(u8_rank, u8_file, e_color);
}

#ifdef LED_TESTS

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

void test2(void)
{
    if (btn::pb1.released())
    {
        //LOGD("pb1 count %u", btn::pb1.getCount());
        if (++u8_test_idx >= sizeof(au8_test_rgb)) {
            u8_test_idx = 0;
        }
        LOGD("test idx = %u", u8_test_idx);
    }
    if (btn::pb2.released() || (btn::pb2.pressedDuration() > 100))
    {
        //LOGD("pb2 count %u", btn::pb2.getCount());
        btn::pb2.resetCount();
        if (au8_test_rgb[u8_test_idx] <= 8) {
            au8_test_rgb[u8_test_idx]  = 0;
        } else {
            au8_test_rgb[u8_test_idx] -= 2;
        }
        LOGD("colors [%u] = (%02x %02x %02x) (%02x %02x %02x)", u8_test_idx,
            au8_test_rgb[0], au8_test_rgb[1], au8_test_rgb[2],
            au8_test_rgb[3], au8_test_rgb[4], au8_test_rgb[5]);
    }
    if (btn::pb3.released() || (btn::pb3.pressedDuration() > 100))
    {
        //LOGD("pb3 count %u", btn::pb3.getCount());
        btn::pb3.resetCount();
        if (au8_test_rgb[u8_test_idx] >= (255-8)) {
            au8_test_rgb[u8_test_idx]  = 255;
        } else {
            au8_test_rgb[u8_test_idx] += 2;
        }
        LOGD("colors [%u] = (%02x %02x %02x) (%02x %02x %02x)", u8_test_idx,
            au8_test_rgb[0], au8_test_rgb[1], au8_test_rgb[2],
            au8_test_rgb[3], au8_test_rgb[4], au8_test_rgb[5]);
    }

    static uint32_t prev_ms = 0;
    if (millis() - prev_ms > 250)
    {
        prev_ms = millis();
        for (uint8_t rank = 1; rank < 4; rank++)
            for (uint8_t file = 1; file < 4; file++)
                setColor(rank, file, LED_OFF);
        pixels.show();
    }
}

#endif // LED_TESTS

} // namespace ui::leds
