
#include "globals.h"
#include "display.h"


namespace ui::display
{


Adafruit_SH1106G oled(SCREEN_WIDTH, SCREEN_HEIGHT);
SemaphoreHandle_t mtx = NULL;


bool init(void)
{
    if (NULL == mtx)
    {
        mtx = xSemaphoreCreateMutex();
        ASSERT((NULL != mtx), "display mutex allocation failed");

        oled.begin();
    }

    if (!oled.init())
    {
        LOGE("OLED not found");
        return false;
    }
    else
    {
        oled.splash();
        oled.display();
        LOGD("OLED ok");
    }

    return true;
}

} // namespace ui::display
