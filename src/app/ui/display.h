
#pragma once

#include <oledmono/sh110x.h>


namespace ui::display
{

extern SH1106G          oled;

#define SCREEN_WIDTH                            128     // OLED display width, in pixels
#define SCREEN_HEIGHT                           64      // OLED display height, in pixels
#define SCREEN_ADDRESS                          0x3C    // OLED I2C device address


#define DISPLAY_DO(func, ...)                   if (ui::display::lock()) {                      \
                                                ui::display::oled.func(__VA_ARGS__);            \
                                                ui::display::unlock(); }

#define DISPLAY_SHOW()                          DISPLAY_DO(display)
#define DISPLAY_CLEAR()                         DISPLAY_DO(clearDisplay)

#define DISPLAY_FILL_RECT(x, y, w, h, col)      if (ui::display::lock()) {                      \
                                                ui::display::oled.fillRect(x, y, w, h, col);    \
                                                ui::display::oled.display();                    \
                                                ui::display::unlock(); }

#define DISPLAY_CLEAR_RECT(x, y, w, h)          DISPLAY_FILL_RECT(x, y, w, h, SH110X_BLACK)
#define DISPLAY_CLEAR_ROW(y, h)                 DISPLAY_CLEAR_RECT(0, y, SCREEN_WIDTH-1, h)


#define DISPLAY_TEXT(x, y, fnt, msg, ...)       if (ui::display::lock()) {                      \
                                                ui::display::oled.setFont(fnt);                 \
                                                ui::display::oled.setCursor(x, y);              \
                                                ui::display::oled.print(msg, ## __VA_ARGS__);   \
                                                ui::display::oled.display();                    \
                                                ui::display::unlock(); }

// font1 (mono) h = 8
#define DISPLAY_TEXT1(x, y, msg, ...)           DISPLAY_TEXT(x, y, NULL, msg, ## __VA_ARGS__)
// font2 (sans) h = 14
#define DISPLAY_TEXT2(x, y, msg, ...)           DISPLAY_TEXT(x, y + 13, ui::display::font2, msg, ## __VA_ARGS__)

bool init();
bool lock();
void unlock();

// via voltage divider resistors
#define BATT_ADC_R1             (3270)      // 3k3 (from BATT+)
#define BATT_ADC_R2             (4780)      // 4k7 (to GND)
#define BATT_LEVEL_MIN          (3400)      // int millivolts

void showBattLevel(void);
bool battLevelOk(void);

} // namespace ui::display
