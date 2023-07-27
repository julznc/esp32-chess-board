
#include "globals.h"
#include "buttons.h"
#include "ledstrip.h"


namespace ui::leds
{

Adafruit_NeoPixel pixels(LED_STRIP_NUMPIXELS, LED_STRIP_PIN, NEO_GRB + NEO_KHZ800);


#ifdef LED_TESTS

static uint8_t au8_test_rgb[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
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

void setColor(uint8_t u8_rank, uint8_t u8_file, led_color_et e_color)
{
#ifdef LED_TESTS
    /*
      red:      a7 00 00  -  ff 00 00
      orange:   37 30 00  -  40 9f 00
      yellow:   27 57 00  -  18 ff 00
      green:    09 5f 00  -  00 ff 00
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
        { 0xa7, 0x00, 0x00, 0xff, 0x00, 0x00 }, // red
        { 0x37, 0x30, 0x00, 0x40, 0x9f, 0x00 }, // orange
        { 0x27, 0x57, 0x00, 0x18, 0xff, 0x00 }, // yellow
        { 0x09, 0x5f, 0x00, 0x00, 0xff, 0x00 }, // green
    };

    uint8_t u8_idx = 0;
    uint8_t u8_offset = 0;

    if ((e_color >= LED_RED) && (e_color <= LED_GREEN)) {
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
    if (btn::pb2.released())
    {
        //LOGD("pb2 count %u", btn::pb2.getCount());
        if (au8_test_rgb[u8_test_idx] <= 8) {
            au8_test_rgb[u8_test_idx]  = 0;
        } else {
            au8_test_rgb[u8_test_idx] -= 8;
        }
        LOGD("colors = (%02x %02x %02x) (%02x %02x %02x)",
            au8_test_rgb[0], au8_test_rgb[1], au8_test_rgb[2],
            au8_test_rgb[3], au8_test_rgb[4], au8_test_rgb[5]);
    }
    if (btn::pb3.released())
    {
        //LOGD("pb3 count %u", btn::pb3.getCount());
        if (au8_test_rgb[u8_test_idx] >= (255-8)) {
            au8_test_rgb[u8_test_idx]  = 255;
        } else {
            au8_test_rgb[u8_test_idx] += 8;
        }
        LOGD("colors = (%02x %02x %02x) (%02x %02x %02x)",
            au8_test_rgb[0], au8_test_rgb[1], au8_test_rgb[2],
            au8_test_rgb[3], au8_test_rgb[4], au8_test_rgb[5]);
    }
}

#endif // LED_TESTS

} // namespace ui::leds
