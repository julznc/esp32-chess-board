
#include "globals.h"
#include "wifi/wifi_setup.h"
#include "lichess_api.h"


namespace lichess
{

static enum {
    CLIENT_STATE_INIT,
    CLIENT_STATE_IDLE
} e_state;

static void taskClient(void *)
{
    WDT_WATCH(NULL);

    for (;;)
    {
        WDT_FEED();

        switch (e_state)
        {
        case CLIENT_STATE_INIT:
            break;

        case CLIENT_STATE_IDLE:
            break;

        default:
            e_state = CLIENT_STATE_INIT;
            break;
        }

        delay(10);
    }

}

void init(void)
{
    e_state = CLIENT_STATE_INIT;

    xTaskCreate(
        taskClient,     /* Task function. */
        "taskClient",   /* String with name of task. */
        32*1024,        /* Stack size in bytes. */
        NULL,           /* Parameter passed as input of the task */
        4,              /* Priority of the task. */
        NULL);          /* Task handle. */
}


} // namespace lichess
