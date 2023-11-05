
#include "globals.h"

#include "display.h"


namespace ui::display
{

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

    return true;
}

} // namespace ui::display
