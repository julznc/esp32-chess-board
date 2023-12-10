
#include "globals.h"

#include <driver/rmt_tx.h>

#include "leds.h"


namespace ui::leds
{

static bool init_done = false;


#define RMT_LED_STRIP_RESOLUTION_HZ     (1 * 1000 * 10000) // 10MHz resolution

static rmt_channel_handle_t led_channel = NULL;
static rmt_encoder_handle_t led_encoder = NULL;
static rmt_transmit_config_t led_tx_cfg;
static uint8_t              led_strip_pixels[LED_STRIP_NUMPIXELS * 3 /*rgb bytes*/];
static SemaphoreHandle_t    mtx = NULL;


bool init(void)
{
    if (!init_done)
    {
        mtx = xSemaphoreCreateMutex();
        assert(NULL != mtx);

        rmt_tx_channel_config_t tx_ch_cfg;
        tx_ch_cfg.gpio_num = PIN_LED_STRIP;
        tx_ch_cfg.clk_src = RMT_CLK_SRC_DEFAULT;
        tx_ch_cfg.resolution_hz = RMT_LED_STRIP_RESOLUTION_HZ; // 10MHz
        tx_ch_cfg.mem_block_symbols = 128; // min 64 for non-DMA (max 256?)
        tx_ch_cfg.trans_queue_depth = 4;
        tx_ch_cfg.flags.invert_out = 0;
        tx_ch_cfg.flags.with_dma = 0; // no RMT-DMA support for esp32s2
        tx_ch_cfg.flags.io_loop_back = 0;
        tx_ch_cfg.flags.io_od_mode = 0;

        led_strip_encoder_config_t encoder_cfg;
        encoder_cfg.resolution = RMT_LED_STRIP_RESOLUTION_HZ;

        ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_ch_cfg, &led_channel));
        ESP_ERROR_CHECK(rmt_new_led_strip_encoder(&encoder_cfg, &led_encoder));
        ESP_ERROR_CHECK(rmt_enable(led_channel));

        memset(led_strip_pixels, 0, sizeof(led_strip_pixels));
        led_tx_cfg.loop_count = 0, // no transfer loop
        led_tx_cfg.flags.eot_level = 0;
        init_done = true;

        update();
    }

    return init_done;
}

void update(void)
{
    if (init_done && (pdTRUE == xSemaphoreTake(mtx, portMAX_DELAY)))
    {
        // Flush RGB values to LEDs
        ESP_ERROR_CHECK(rmt_transmit(led_channel, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &led_tx_cfg));
        ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_channel, portMAX_DELAY));

        xSemaphoreGive(mtx);
    }
}

void clear(void)
{
    if (init_done && (pdTRUE == xSemaphoreTake(mtx, portMAX_DELAY)))
    {
        memset(led_strip_pixels, 0, sizeof(led_strip_pixels));
        xSemaphoreGive(mtx);
    }
}

static void led_strip_set_pixel(uint16_t index, uint8_t red, uint8_t green, uint8_t blue)
{
    uint8_t *pixel = led_strip_pixels;

    pixel += index * 3;
    // GRB format
    *pixel++ = green;
    *pixel++ = red;
    *pixel++ = blue;
}


void setColor(uint8_t u8_rank, uint8_t u8_file, uint8_t u8_red, uint8_t u8_green, uint8_t u8_blue)
{
    uint16_t u16_mid     = (uint16_t)(u8_file >> 2) << 6;
    uint16_t u16_offset  = u16_mid + ((u8_rank) << 3);

    u8_file &= 0x03; // %4

    if (init_done && (pdTRUE == xSemaphoreTake(mtx, portMAX_DELAY)))
    {
        led_strip_set_pixel(u16_offset + u8_file,     u8_red, u8_green, u8_blue);
        led_strip_set_pixel(u16_offset + 7 - u8_file, u8_red, u8_green, u8_blue);
        xSemaphoreGive(mtx);
    }
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
