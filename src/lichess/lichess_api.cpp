
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

#include "globals.h"
#include "ui/ui.h"
#include "wifi/wifi_setup.h"

#include "lichess_api_cfg.h"
#include "lichess_api.h"


namespace lichess
{

WiFiClientSecure    client;
HTTPClient          https;


static enum {
    CLIENT_STATE_INIT,
    CLIENT_STATE_ACCOUNT,   // check lichess account/profile
    CLIENT_STATE_IDLE
} e_state;

static void taskClient(void *)
{
    WDT_WATCH(NULL);

    client.setCACert(LICHESS_ORG_PEM);

    for (;;)
    {
        WDT_FEED();

        switch (e_state)
        {
        case CLIENT_STATE_INIT:
            if (wifi::is_ntp_connected())
            {
                e_state = CLIENT_STATE_ACCOUNT;
            }
            break;

        case CLIENT_STATE_ACCOUNT:
            if (ui::btn::pb2.getCount())
            {
                if (!https.begin(client, "https://lichess.org/api/account"))
                {
                    LOGW("failed https.begin()");
                }
                else
                {
                    String auth = "Bearer ";
                    auth += LICHESS_API_ACCESS_TOKEN;
                    https.addHeader("Authorization", auth);
                    LOGD("[HTTPS] GET...");
                    int httpCode = https.GET();
                    LOGD("GET() = %d", httpCode);
                    if (httpCode > 0)
                    {
                        if ((HTTP_CODE_OK == httpCode) || (HTTP_CODE_MOVED_PERMANENTLY == httpCode))
                        {
                            String payload = https.getString();
                            LOGD("payload:\r\n%s", payload.c_str());
                        }
                        else
                        {
                            LOGW("GET failed: %s", https.errorToString(httpCode).c_str());
                        }
                    }
                    https.end();
                }

                delay(2000UL);
            }
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
