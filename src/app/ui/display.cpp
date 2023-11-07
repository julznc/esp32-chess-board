
#include "globals.h"

#include "display.h"


namespace ui::display
{

SH1106G oled(SCREEN_WIDTH, SCREEN_HEIGHT);
static SemaphoreHandle_t mtx             = NULL;


bool init()
{
    //LOGD("%s", __PRETTY_FUNCTION__);
    if (NULL == mtx)
    {
        mtx = xSemaphoreCreateMutex();
        assert(NULL != mtx);

        hal_i2c_init();
    }

    oled.init();

    return true;
}

} // namespace ui::display
