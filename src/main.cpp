
#include "globals.h"
#include "board/board.h"
#include "lichess/lichess_api.h"
#include "ui/ui.h"
#include "wifi/wifi_setup.h"


void setup()
{
    global_init();

    WDT_WATCH(NULL);

    ui::init();

    while (!ui::display::battLevelOk()) {
        delay(100); // pause
    }

    brd::init();
    wifi::setup();
    lichess::init();
}

void loop()
{
    WDT_FEED();
    brd::loop();
    LED_TOGGLE();
}
