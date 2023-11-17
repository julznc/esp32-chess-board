
#include "globals.h"

#include "board/board.h"
#include "lichess/lichess_client.h"
#include "ui/ui.h"
#include "wifi/wifi_setup.h"


#define DECLARE_TASK(task, _setup, _loop, _delay)               \
            static esp_task_wdt_user_handle_t task##_wdt_hdl;   \
            static void task##Task(void *arg) { \
                _setup(); for (;;) {            \
                    _loop(); delayms(_delay);   \
                    esp_task_wdt_reset_user(task##_wdt_hdl); \
                }}


DECLARE_TASK(Board,     brd::init,      brd::loop,      2);
DECLARE_TASK(Client,    lichess::init,  lichess::loop, 10);
DECLARE_TASK(Ui,        ui::init,       ui::loop,       5);
DECLARE_TASK(Wifi,      wifi::init,     wifi::loop,    10);


extern "C" void app_main(void)
{
    global_init();

  #define RUN_TASK(task, stack, priority)                                        \
                ESP_ERROR_CHECK(esp_task_wdt_add_user(#task, &task##_wdt_hdl)); \
                assert(pdTRUE == xTaskCreatePinnedToCore(task##Task, #task, (stack), NULL, (priority), NULL, 0))

    RUN_TASK(Ui,        4*1024, 3);

    while (!BATT_OK()) {
        delayms(100); // pause
    }

    RUN_TASK(Board,     4*1024, 5);
    RUN_TASK(Wifi,      8*1024, 4);
    RUN_TASK(Client,    8*1024, 7);
}
