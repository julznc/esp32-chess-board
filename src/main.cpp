
#include "globals.h"
#include "ui/ui.h"


void setup()
{
    global_init();

    WDT_WATCH(NULL);

    ui::init();
}

void loop()
{
    LED_ON();
    delay(500);
    LED_OFF();
    delay(500);
    WDT_FEED();
}
