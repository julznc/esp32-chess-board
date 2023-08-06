
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
    static uint32_t ms_batt_update = 0;
    switch (e_state)
    {
    case UI_STATE_INIT:
        if (!btn::init() || !display::init() || !leds::init())
        {
            delay(5*1000UL);
        }
        else
        {
            delay(1*1000UL);
            DISPLAY_TEXT1(0, 0, "e-Board v%s", K_APP_VERSION);
            delay(1*1000UL);
            DISPLAY_CLEAR();
            DISPLAY_TEXT1(0, 0, "e-Board v%.5s", K_APP_VERSION);
            //DISPLAY_TEXT2(20, 6, "electronic");
            //DISPLAY_TEXT2(10, 24, "chess board");
            //DISPLAY_TEXT2(30, 42, "v01.00");

            e_state = UI_STATE_IDLE;
        }
        break;

    case UI_STATE_IDLE:
        if (millis() - ms_batt_update > 1500UL)
        {
            ms_batt_update = millis();
            display::showBattLevel();
        }
        btn::loop();
        //leds::test();
        //leds::test2();
        break;

    default:
        e_state = UI_STATE_INIT;
        break;
    }
}

static void taskUI(void *)
{
    WDT_WATCH(NULL);

    for(;;)
    {
        WDT_FEED();

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
