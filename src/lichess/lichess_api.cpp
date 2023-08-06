
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

#include "globals.h"
#include "ui/ui.h"
#include "wifi/wifi_setup.h"

#include "lichess_api_cfg.h"
#include "lichess_api.h"


#define SHOW_STATUS(msg, ...)       DISPLAY_CLEAR_ROW(20, 8); \
                                    DISPLAY_TEXT1(0, 20, msg, ## __VA_ARGS__)


namespace lichess
{

String              auth;
WiFiClientSecure    client;
HTTPClient          https;
DynamicJsonDocument response(4*1024);


static enum {
    CLIENT_STATE_INIT,
    CLIENT_STATE_ACCOUNT,   // check lichess account/profile
    CLIENT_STATE_IDLE
} e_state;


static bool get_account()
{
    if (!https.begin(client, "https://lichess.org/api/account"))
    {
        LOGW("failed https.begin()");
    }
    else
    {
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
                response.clear();
                DeserializationError error = deserializeJson(response, payload);
                if (error)
                {
                    LOGW("deserializeJson() failed", error.f_str());
                }
                else
                {
                    const char *username = response["username"].as<const char*>();
                    LOGI("username : %s", username);
                    SHOW_STATUS("user: %s", username);
                }
            }
            else
            {
                LOGW("GET failed: %s", https.errorToString(httpCode).c_str());
            }
        }
        https.end();
    }
    return false;
}

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
                auth  = "Bearer ";
                auth += LICHESS_API_ACCESS_TOKEN;

                e_state = CLIENT_STATE_ACCOUNT;
            }
            break;

        case CLIENT_STATE_ACCOUNT:
            if (ui::btn::pb2.getCount())
            {
                get_account();

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

    // use default memory allocation - can also handle PSRAM up to 4MB
    mbedtls_platform_set_calloc_free(calloc, free);

#if 1
    xTaskCreate(
        taskClient,     /* Task function. */
        "taskClient",   /* String with name of task. */
        32*1024,        /* Stack size in bytes. */
        NULL,           /* Parameter passed as input of the task */
        4,              /* Priority of the task. */
        NULL);          /* Task handle. */

#else // panic reset! don't use
    StaticTask_t xTaskBuffer;
    const uint32_t u32_stack_depth = 32*1024;
    StackType_t *xStack = (StackType_t *)heap_caps_calloc(1, u32_stack_depth, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT | MALLOC_CAP_32BIT);;
    ASSERT(NULL != xStack, "heap_caps_calloc(%lu) failed", u32_stack_depth);
    xTaskCreateStatic(taskClient, "taskClient", u32_stack_depth, NULL, 3, xStack, &xTaskBuffer);
#endif
}


} // namespace lichess
