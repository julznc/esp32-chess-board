
#include "globals.h"

#include "leds.h"


namespace ui::leds
{

static led_strip_handle_t led_strip;

bool init(void)
{
    static bool done = false;

    if (!done)
    {
        /* LED strip initialization with the GPIO and pixels number*/
        led_strip_config_t strip_config;// = {0, };
        strip_config.strip_gpio_num = PIN_LED_STRIP;
        strip_config.max_leds = LED_STRIP_NUMPIXELS;
        strip_config.led_pixel_format = LED_PIXEL_FORMAT_GRB;
        strip_config.led_model = LED_MODEL_WS2812;
        strip_config.flags.invert_out = 0;

        led_strip_rmt_config_t rmt_config;// = {0, };
        rmt_config.clk_src = RMT_CLK_SRC_DEFAULT,
        rmt_config.resolution_hz = 10 * 1000 * 1000; // 10MHz
        rmt_config.mem_block_symbols = 0; // at least 64, or 0 to set default
        rmt_config.flags.with_dma = 0; // no DMA (not supported)

        ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));

        /* Set all LED off to clear all pixels */
        led_strip_clear(led_strip);

        done = true;
    }

    return done;
}

} // namespace ui::leds
