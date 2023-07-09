#ifndef __UI_LEDSTRIP_H__
#define __UI_LEDSTRIP_H__

#include <Adafruit_NeoPixel.h>

namespace ui::leds
{

#define LED_STRIP_NUMPIXELS         (8 * 8 * 2)


bool init(void);

} // namespace ui::leds

#endif // __UI_LEDSTRIP_H__
