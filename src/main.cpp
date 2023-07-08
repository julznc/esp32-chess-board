
#include <Adafruit_SH110X.h>
#include <Adafruit_NeoPixel.h>


#define SCREEN_WIDTH            128   // OLED display width, in pixels
#define SCREEN_HEIGHT           64    // OLED display height, in pixels
#define SCREEN_SDA_PIN          (9)
#define SCREEN_SCL_PIN          (10)

#define LED_STRIP_PIN           (21)
#define LED_STRIP_NUMPIXELS     (8 * 8 * 2)


Adafruit_SH1106G oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire);
Adafruit_NeoPixel pixels(LED_STRIP_NUMPIXELS, LED_STRIP_PIN, NEO_GRB + NEO_KHZ800);


void setup()
{
    pinMode(LED_BUILTIN, OUTPUT);

    Wire.setPins(SCREEN_SDA_PIN, SCREEN_SCL_PIN);
    oled.begin();

    oled.display();
    pixels.begin();
    pixels.fill(pixels.Color(0, 0, 0), 0, LED_STRIP_NUMPIXELS);
    pixels.show();
}

void loop()
{
    digitalWrite(LED_BUILTIN, HIGH);
    delay(500);
    digitalWrite(LED_BUILTIN, LOW);
    delay(500);
}
