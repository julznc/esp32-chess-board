
#include "globals.h"
#include "ui.h"

namespace ui
{

static enum {
    UI_STATE_INIT,
    UI_STATE_IDLE
} e_state;

static void cycle(void)
{
    switch (e_state)
    {
    case UI_STATE_INIT:
        if (!display::init() || !leds::init())
        {
            delay(5*1000UL);
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

static void taskUI(void *)
{
    for(;;)
    {
        cycle();
        delay(5);
    }
}

void init(void)
{
    e_state = UI_STATE_INIT;

    xTaskCreate(
        taskUI,         /* Task function. */
        "taskUI",       /* String with name of task. */
        16*1024,        /* Stack size in bytes. */
        NULL,           /* Parameter passed as input of the task */
        4,              /* Priority of the task. */
        NULL);          /* Task handle. */
}

} // namespace ui
