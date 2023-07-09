
#include "globals.h"
#include "ledstrip.h"


namespace ui::leds
{

Adafruit_NeoPixel pixels(LED_STRIP_NUMPIXELS, LED_STRIP_PIN, NEO_GRB + NEO_KHZ800);

bool init(void)
{
    pixels.begin();
    pixels.fill(pixels.Color(0, 0, 0), 0, LED_STRIP_NUMPIXELS);
    pixels.show();
    return true;
}

} // namespace ui::leds
