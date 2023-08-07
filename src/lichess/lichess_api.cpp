
#include "globals.h"
#include "ui/ui.h"
#include "wifi/wifi_setup.h"

#include "lichess_api_cfg.h"
#include "lichess_api.h"


#define SHOW_STATUS(msg, ...)       DISPLAY_CLEAR_ROW(20, 8); \
                                    DISPLAY_TEXT1(0, 20, msg, ## __VA_ARGS__)


namespace lichess
{

ApiClient::ApiClient() : HTTPClient(),
    _auth("Bearer "), _url(LICHESS_API_URL_PREFIX)
{
    // to do: load from preferences
    _client.setCACert(LICHESS_ORG_PEM);
    _auth += LICHESS_API_ACCESS_TOKEN;
}

bool ApiClient::begin(const char *endpoint)
{
    return HTTPClient::begin(_client, _url + endpoint);
}

int ApiClient::sendRequest(const char *type, uint8_t *payload, size_t size)
{
    HTTPClient::addHeader("Authorization", _auth);
    return HTTPClient::sendRequest(type, payload, size);
}

bool ApiClient::startStream(const char *endpoint)
{
    int     code        = 0;
    bool    b_status    = false;

    // to do: handle redirects, if any

    if (begin(endpoint))
    {
        HTTPClient::addHeader("Authorization", _auth);

        // wipe out any existing headers from previous request
        for(size_t i = 0; i < _headerKeysCount; i++) {
            if (_currentHeaders[i].value.length() > 0) {
                _currentHeaders[i].value.clear();
            }
        }

        if (!connect())
        {
            LOGW("connect() failed");
        }
        else if (!sendHeader("GET"))
        {
            LOGW("sendHeader() failed");
        }
        else if ((code = handleHeaderResponse()) > 0)
        {
            b_status = (HTTP_CODE_OK == code);
        }
    }

    return b_status;
}

String ApiClient::streamResponse()
{
    if (_client.available())
    {
        String line = _client.readStringUntil('\n');
        line.trim(); // remove \r
        return line;
    }
    return "";
}


ApiClient           client;
DynamicJsonDocument response(2*1024);


static enum {
    CLIENT_STATE_INIT,
    CLIENT_STATE_GET_ACCOUNT,   // check lichess account/profile
    CLIENT_STATE_POLL_EVENTS,   // stream incoming events
    CLIENT_STATE_IDLE
} e_state;


static bool get_account()
{
    bool b_status = false;

    SHOW_STATUS("lichess.org ...");
    if (!api_get("/account", response))
    {
        SHOW_STATUS("lichess response error");
    }
    else if (!response.containsKey("username"))
    {
        LOGW("unknown username");
        SHOW_STATUS("unknown username");
    }
    else
    {
        const char *username = response["username"].as<const char*>();
        LOGI("username : %s", username);
        SHOW_STATUS("user: %s", username);
        b_status = true;
    }

    return b_status;
}

static int poll_events()
{
    String          payload;
    challenge_st    s_challenge;
    int             result = -1;

    if (client.connected())
    {
        payload = client.streamResponse();
        result  = 0;
        if (payload.length() > 1)
        {
            LOGD("payload (%u): %s", payload.length(), payload.c_str());
            response.clear();
            DeserializationError error = deserializeJson(response, payload);
            if (error)
            {
                LOGW("deserializeJson() failed: %s", error.f_str());
            }
            else if (!response.containsKey("type"))
            {
                LOGW("unknown event type");
            }
            else
            {
                const char *type = response["type"].as<const char*>();
                LOGD("event %s", type);
                if (0 == strncmp(type, "challenge", strlen("challenge")))
                {
                    result = parse_challenge_event(response, &s_challenge);
                    if (CHALLENGE_CREATED == result)
                    {
                        LOGI("incoming %s challenge '%s' from '%s' (%s %u+%u)", s_challenge.b_rated ? "rated" : "casual",
                            s_challenge.ac_id, s_challenge.ac_user, s_challenge.b_color ? "white" : "black",
                            s_challenge.u16_clock_limit/60, s_challenge.u8_clock_increment);
                    }
                    else //if (result)
                    {
                        LOGD("challenge -> %d", result);
                    }
                }
            }
        }

    }

    return result;
}

static void taskClient(void *)
{
    WDT_WATCH(NULL);

    delay(2500UL);

    for (;;)
    {
        WDT_FEED();

        switch (e_state)
        {
        case CLIENT_STATE_INIT:
            if (wifi::is_ntp_connected())
            {
                e_state = CLIENT_STATE_GET_ACCOUNT;
            }
            else
            {
                SHOW_STATUS("lichess offline");
                delay(1500UL);
            }
            break;

        case CLIENT_STATE_GET_ACCOUNT:
            if (get_account())
            {
                if (client.startStream("/stream/event"))
                {
                    e_state = CLIENT_STATE_POLL_EVENTS;
                }
                else
                {
                    e_state = CLIENT_STATE_IDLE;
                }
            }
            break;

        case CLIENT_STATE_POLL_EVENTS:
            if (poll_events() < 0)
            {
                e_state = CLIENT_STATE_INIT;
            }
            else
            {
                e_state = CLIENT_STATE_IDLE;
            }
            break;

        case CLIENT_STATE_IDLE:
            e_state = CLIENT_STATE_POLL_EVENTS;
            break;

        default:
            e_state = CLIENT_STATE_INIT;
            break;
        }

        delay(10);
    }

}

/*
 * Public Functions
 */

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

/*
 * Private Functions
 */
bool api_get(const char *endpoint, DynamicJsonDocument &json, bool b_debug)
{
    bool    b_status = false;

    if (!client.begin(endpoint))
    {
        LOGW("begin(%s) failed", endpoint);
    }
    else
    {
        int httpCode = client.sendRequest("GET");
        if (httpCode > 0)
        {
            String payload = client.getString();
            if (b_debug) {
                LOGD("payload (%u):\r\n%s", payload.length(), payload.c_str());
            }

            if ((HTTP_CODE_OK == httpCode) || (HTTP_CODE_MOVED_PERMANENTLY == httpCode))
            {
                if (payload.length() > 0)
                {
                    json.clear();
                    DeserializationError error = deserializeJson(json, payload);
                    if (error)
                    {
                        LOGW("deserializeJson() failed: %s", error.f_str());
                    }
                    else
                    {
                        b_status = true;
                    }
                }
            }
            else
            {
                LOGW("GET %s failed: %s", endpoint, client.errorToString(httpCode).c_str());
            }
        }
        else
        {
            LOGW("error code %d", httpCode);
        }
    }

    client.end();

    return b_status;
}

} // namespace lichess
