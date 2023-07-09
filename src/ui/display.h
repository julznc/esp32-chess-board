#ifndef __UI_DISPLAY_H__
#define __UI_DISPLAY_H__

#include <Adafruit_SH110X.h>


namespace ui::display
{

#define SCREEN_WIDTH                128   // OLED display width, in pixels
#define SCREEN_HEIGHT               64    // OLED display height, in pixels

extern Adafruit_SH1106G oled;
extern SemaphoreHandle_t mtx;


bool init(void);

} // namespace ui::display

#endif // __UI_DISPLAY_H__
