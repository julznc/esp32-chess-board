#ifndef __UI_DISPLAY_H__
#define __UI_DISPLAY_H__

#include <Adafruit_SH110X.h>


namespace ui::display
{

#define SCREEN_WIDTH                            128   // OLED display width, in pixels
#define SCREEN_HEIGHT                           64    // OLED display height, in pixels

extern Adafruit_SH1106G         oled;
extern SemaphoreHandle_t        mtx;
extern const GFXfont           *font2;


#define DISPLAY_LOCK()                          xSemaphoreTake(ui::display::mtx, portMAX_DELAY)
#define DISPLAY_UNLOCK()                        xSemaphoreGive(ui::display::mtx)

#define DISPLAY_DO(func, ...)                   DISPLAY_LOCK();                                 \
                                                ui::display::oled.func(__VA_ARGS__);            \
                                                DISPLAY_UNLOCK()

#define DISPLAY_SHOW()                          DISPLAY_DO(display)
#define DISPLAY_CLEAR()                         DISPLAY_DO(clearDisplay)

#define DISPLAY_FILL_RECT(x, y, w, h, col)      DISPLAY_LOCK();  \
                                                ui::display::oled.fillRect(x, y, w, h, col);    \
                                                ui::display::oled.display();                    \
                                                DISPLAY_UNLOCK()

#define DISPLAY_CLEAR_RECT(x, y, w, h)          DISPLAY_FILL_RECT(x, y, w, h, SH110X_BLACK)
#define DISPLAY_CLEAR_ROW(y, h)                 DISPLAY_CLEAR_RECT(0, y, SCREEN_WIDTH-1, h)


#define DISPLAY_TEXT(x, y, fnt, msg, ...)       DISPLAY_LOCK();                                 \
                                                ui::display::oled.setFont(fnt);                 \
                                                ui::display::oled.setCursor(x, y);              \
                                                ui::display::oled.printf(msg, ## __VA_ARGS__);  \
                                                ui::display::oled.display();                    \
                                                DISPLAY_UNLOCK()

// font1 (mono) h = 8
#define DISPLAY_TEXT1(x, y, msg, ...)           DISPLAY_TEXT(x, y, NULL, msg, ## __VA_ARGS__)
// font2 (sans) h = 14
#define DISPLAY_TEXT2(x, y, msg, ...)           DISPLAY_TEXT(x, y + 13, ui::display::font2, msg, ## __VA_ARGS__)


bool init(void);

} // namespace ui::display

#endif // __UI_DISPLAY_H__