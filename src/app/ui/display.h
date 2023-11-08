
#pragma once

#include <oledmono/sh110x.h>


namespace ui::display
{

extern SH1106G          oled;

#define SCREEN_WIDTH                            128     // OLED display width, in pixels
#define SCREEN_HEIGHT                           64      // OLED display height, in pixels
#define SCREEN_ADDRESS                          0x3C    // OLED I2C device address


bool init();

} // namespace ui::display
