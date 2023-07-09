
#include <Adafruit_SH110X.h>
#include <Adafruit_NeoPixel.h>

#include "globals.h"


#define SCREEN_WIDTH            128   // OLED display width, in pixels
#define SCREEN_HEIGHT           64    // OLED display height, in pixels

#define LED_STRIP_PIN           (21)
#define LED_STRIP_NUMPIXELS     (8 * 8 * 2)


Adafruit_SH1106G oled(SCREEN_WIDTH, SCREEN_HEIGHT);
Adafruit_NeoPixel pixels(LED_STRIP_NUMPIXELS, LED_STRIP_PIN, NEO_GRB + NEO_KHZ800);


void setup()
{
    global_init();

    oled.begin();

    oled.display();
    pixels.begin();
    pixels.fill(pixels.Color(0, 0, 0), 0, LED_STRIP_NUMPIXELS);
    pixels.show();
}

void loop()
{
    LED_ON();
    delay(500);
    LED_OFF();
    delay(500);
}
