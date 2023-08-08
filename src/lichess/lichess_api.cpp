
#include "globals.h"
#include "ui/ui.h"
#include "wifi/wifi_setup.h"

#include "lichess_api_cfg.h"
#include "lichess_api.h"


#define SHOW_STATUS(msg, ...)       DISPLAY_CLEAR_ROW(20, 8); \
                                    DISPLAY_TEXT1(0, 20, msg, ## __VA_ARGS__)


namespace lichess
{

static ApiClient        main_client(false);     // main connection (non-stream)
static ApiClient        events_client(true);    // events-stream connection
static ApiClient        board_client(true);     // game_state-stream connection
static challenge_st     s_incoming_challenge;
static game_st          s_current_game;

static char             ac_username[32];
DynamicJsonDocument     response(2*1024);


static enum {
    CLIENT_STATE_INIT,
    CLIENT_STATE_GET_ACCOUNT,   // check lichess account/profile
    CLIENT_STATE_CHECK_EVENTS,  // check messages from eventd-stream
    CLIENT_STATE_CHECK_GAME,    // check messages from game-state stream
    CLIENT_STATE_IDLE
} e_state;


ApiClient::ApiClient(bool b_stream) : HTTPClient(),
    _auth("Bearer "), _url(LICHESS_API_URL_PREFIX)
{
    setReuse(!b_stream);

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


static bool get_account()
{
    bool b_status = false;

    SHOW_STATUS("lichess.org ...");

    memset(ac_username, 0, sizeof(ac_username));

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
        strncpy(ac_username, username, sizeof(ac_username) - 1);
        SHOW_STATUS("user: %.*s", 14, ac_username);
        b_status = true;
    }

    return b_status;
}

static int poll_events()
{
    String  payload;
    int     result = -1;

    if (events_client.connected())
    {
        payload = events_client.streamResponse();
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
                if (0 == strncmp(type, "game", strlen("game")))
                {
                    result = parse_game_event(response, &s_current_game);
                    DISPLAY_CLEAR_ROW(45, SCREEN_HEIGHT-45);
                    if ((GAME_STATE_STARTED == result) && s_current_game.ac_id[0])
                    {
                        DISPLAY_TEXT1(0, 54, "<-Abort      Resign->");
                        // ignore any incoming challenge
                        memset(&s_incoming_challenge, 0, sizeof(s_incoming_challenge));
                    }
                }
                else if (0 == strncmp(type, "challenge", strlen("challenge")))
                {
                    challenge_st *pc = &s_incoming_challenge;
                    result = parse_challenge_event(response, pc);
                    if ((CHALLENGE_CREATED == result) && pc->ac_id[0] && pc->ac_user[0])
                    {
                        LOGI("incoming %s challenge '%s' from '%s' (%s %u+%u)", pc->b_rated ? "rated" : "casual",
                            pc->ac_id, pc->ac_user, pc->b_color ? "white" : "black",
                            pc->u16_clock_limit/60, pc->u8_clock_increment);
                        if (GAME_STATE_UNKNOWN == s_current_game.e_state)
                        {
                            DISPLAY_CLEAR_ROW(45, SCREEN_HEIGHT-45);
                            DISPLAY_TEXT1(0, 45, "vs %.*s (%c %u+%u)", 7, pc->ac_user, pc->b_color ? 'B' : 'W',
                                        pc->u16_clock_limit/60, pc->u8_clock_increment);
                            DISPLAY_TEXT1(0, 54, "<-Accept    Decline->");
                        }
                        else
                        {
                            memset(s_incoming_challenge.ac_id, 0, sizeof(s_incoming_challenge.ac_id));
                        }
                        result = EVENT_CHALLENGE_INCOMING;
                    }
                    else if (result)
                    {
                        LOGD("challenge -> %d", result);
                        memset(&s_incoming_challenge, 0, sizeof(s_incoming_challenge));
                        DISPLAY_CLEAR_ROW(45, SCREEN_HEIGHT-45);
                        result = EVENT_CHALLENGE_CANCELED;
                    }
                }
            }
        }

    }
    else if (board_client.connected())
    {
        result = 0;
    }

    return result;
}

static int poll_game_state()
{
    String  payload;
    int     result = -1;

    if (board_client.connected())
    {
        payload = board_client.streamResponse();
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
                LOGW("unknown game state");
            }
            else
            {
                const char *type = response["type"].as<const char*>();
                LOGD("state: %s", type);

                parse_game_state(response, &s_current_game);
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

        int event = EVENT_UNKNOWN;

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
                if (events_client.startStream("/stream/event"))
                {
                    e_state = CLIENT_STATE_CHECK_EVENTS;
                }
                else
                {
                    e_state = CLIENT_STATE_IDLE;
                }
            }
            break;

        case CLIENT_STATE_CHECK_EVENTS:
            event = poll_events();
            if (event < 0)
            {
                e_state = CLIENT_STATE_INIT;
            }
            else if (s_current_game.ac_id[0])
            {
                if ((GAME_STATE_STARTED == event) || (!board_client.connected()))
                {
#if 1 // close other stream to freeup resources
                    events_client.end();
                    delay(1000);
#endif
                    // api/board/game/stream/{gameId}
                    String endpoint = "/board/game/stream/";
                    endpoint += (const char *)s_current_game.ac_id;
                    LOGI("monitor game %s", s_current_game.ac_id);
                    board_client.startStream(endpoint.c_str());
                }
                e_state = CLIENT_STATE_CHECK_GAME;
            }
            else
            {
                e_state = CLIENT_STATE_IDLE;
            }
            break;

        case CLIENT_STATE_CHECK_GAME:
            poll_game_state();
            e_state = CLIENT_STATE_IDLE;
            break;

        case CLIENT_STATE_IDLE:
            if (s_current_game.ac_id[0])
            {
                if (ui::btn::pb3.getCount()) {
                    game_resign(s_current_game.ac_id);
                } else if (ui::btn::pb2.getCount()) {
                    game_abort(s_current_game.ac_id);
                }
            }
            else if (s_incoming_challenge.ac_id[0])
            {
                if (ui::btn::pb3.getCount()) {
                    decline_challenge(s_incoming_challenge.ac_id);
                } else if (ui::btn::pb2.getCount()) {
                    accept_challenge(s_incoming_challenge.ac_id);
                }
            }
            e_state = CLIENT_STATE_CHECK_EVENTS;
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
    // use default memory allocation - can also handle PSRAM up to 4MB
    mbedtls_platform_set_calloc_free(calloc, free);

    memset(&s_incoming_challenge, 0, sizeof(s_incoming_challenge));
    memset(&s_current_game, 0, sizeof(s_current_game));
    memset(ac_username, 0, sizeof(ac_username));

    e_state = CLIENT_STATE_INIT;

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

    if (!main_client.begin(endpoint))
    {
        LOGW("begin(%s) failed", endpoint);
    }
    else
    {
        int httpCode = main_client.sendRequest("GET");
        if (httpCode > 0)
        {
            String payload = main_client.getString();
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
                LOGW("GET %s failed: %s", endpoint, main_client.errorToString(httpCode).c_str());
            }
        }
        else
        {
            LOGW("error code %d", httpCode);
        }
    }

    main_client.end();

    return b_status;
}

bool api_post(const char *endpoint, String payload, DynamicJsonDocument &json, bool b_debug)
{
    bool    b_status = false;

    if (!main_client.begin(endpoint))
    {
        LOGW("begin(%s) failed", endpoint);
    }
    else
    {
        int httpCode = main_client.sendRequest("POST", (uint8_t *)payload.c_str(), payload.length());
        if (httpCode > 0)
        {
            String payload = main_client.getString();
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
                LOGW("GET %s failed: %s", endpoint, main_client.errorToString(httpCode).c_str());
            }
        }
        else
        {
            LOGW("error code %d", httpCode);
        }

    }

    main_client.end();

    return b_status;
}

} // namespace lichess
