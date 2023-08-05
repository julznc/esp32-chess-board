
#include "globals.h"
#include "board/board.h"
#include "lichess/lichess_api.h"
#include "ui/ui.h"
#include "wifi/wifi_setup.h"


void setup()
{
    global_init();

    brd::init();
    lichess::init();
    ui::init();
    wifi::setup();
}

void loop()
{
    LED_ON();
    delay(500);
    LED_OFF();
    delay(500);
    WDT_FEED();
}
