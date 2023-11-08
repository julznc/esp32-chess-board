
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
            delayms(1 * 1000);
            DISPLAY_TEXT1(0, 0, "e-Board v%s", K_APP_VERSION);
            delayms(1 * 1000);
            DISPLAY_CLEAR();
            DISPLAY_TEXT1(0, 0, "e-Board v%.5s", K_APP_VERSION);
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
