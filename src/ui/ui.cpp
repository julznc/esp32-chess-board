
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
            delay(3*1000UL);
            DISPLAY_CLEAR();
            DISPLAY_TEXT2(0, 10, "e-Board %02x.%02x", K_FW_VER_MAJOR, K_FW_VER_MINOR);
            //DISPLAY_TEXT2(20, 6, "electronic");
            //DISPLAY_TEXT2(10, 24, "chess board");
            //DISPLAY_TEXT2(30, 42, "v01.00");

            e_state = UI_STATE_IDLE;
        }
        break;

    case UI_STATE_IDLE:
        //leds::test();
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
