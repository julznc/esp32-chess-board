
#include "globals.h"
#include "ui.h"


namespace ui
{

static enum {
    UI_STATE_INIT,
    UI_STATE_IDLE
} e_state;

bool init()
{
    //LOGD("%s()", __func__);

    e_state = UI_STATE_INIT;

    return true;
}

void loop()
{
    switch (e_state)
    {
    case UI_STATE_INIT:
        if (!display::init())
        {
            delayms(5 * 1000);
        }
        else
        {
            e_state = UI_STATE_IDLE;
        }
        break;

    case UI_STATE_IDLE:
        break;

    default:
        e_state = UI_STATE_INIT;
        break;
    }
}

} // namespace ui
