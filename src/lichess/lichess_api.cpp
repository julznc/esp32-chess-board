
#include "globals.h"
#include "chess/chess.h"
#include "ui/ui.h"
#include "wifi/wifi_setup.h"

#include "lichess_api_cfg.h"
#include "lichess_api.h"


#define SHOW_STATUS(msg, ...)       DISPLAY_CLEAR_ROW(10, 8); \
                                    DISPLAY_TEXT1(0, 10, msg, ## __VA_ARGS__)
#define SET_BOTTOM_MENU(msg, ...)   DISPLAY_CLEAR_ROW(54, 9); \
                                    DISPLAY_TEXT1(0, 54, msg, ## __VA_ARGS__)
#define CLEAR_BOTTOM_MENU()         SET_BOTTOM_MENU("")

namespace lichess
{

static ApiClient        main_client;    // main connection (non-stream)
static ApiClient        stream_client;  // both events-stream & board-state-stream connections
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


ApiClient::SecClient::SecClient() : WiFiClientSecure()
{
    // to do: load from preferences
    _CA_cert = LICHESS_ORG_PEM; // setCACert()
}

ApiClient::ApiClient() : HTTPClient(),
    _auth("Bearer "), _url(LICHESS_API_URL_PREFIX)
{
    // to do: load from preferences
    _auth += LICHESS_API_ACCESS_TOKEN;
}

bool ApiClient::begin(const char *endpoint)
{
    return HTTPClient::begin(_secClient, _url + endpoint);
}

void ApiClient::end(bool b_stop)
{
    if (_secClient.available()) {
        _secClient.flush();
    }
    if (b_stop) {
        _secClient.stop();
    }
    clear();
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

    if (!b_status) {
        end(true);
    }

    return b_status;
}

String ApiClient::streamResponse()
{
    if (_secClient.available())
    {
        String line = _secClient.readStringUntil('\n');
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
        SHOW_STATUS("lichess api error");
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
        SHOW_STATUS("%.*s", 20, ac_username);
        b_status = true;
    }

    return b_status;
}

static int poll_events()
{
    String  payload;
    int     result = -1;

    if (!stream_client.getEndpoint().startsWith("/api/stream/event"))
    {
        return  0; // ignore, not for us
    }

    while (stream_client.connected())
    {
        payload = stream_client.streamResponse();
        result  = 0;

        if (0 == payload.length())
        {
            break; // empty
        }

        if (payload.length() > 8 /* {"x":"y"} */)
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
                        SET_BOTTOM_MENU("<-Abort      Resign->");
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
                            DISPLAY_TEXT1(0, 45, "%.*s(%c %u+%u)", 12, pc->ac_user, pc->b_color ? 'B' : 'W',
                                        pc->u16_clock_limit/60, pc->u8_clock_increment);
                            SET_BOTTOM_MENU("<-Accept    Decline->");
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
        delay(100);
    }

    return result;
}

static int poll_game_state()
{
    String  payload;
    int     result = -1;

    if (!stream_client.getEndpoint().startsWith("/api/board/game/stream"))
    {
        return  0; // ignore, not for us
    }

    while (stream_client.connected())
    {
        payload = stream_client.streamResponse();
        result  = 0;

        if (0 == payload.length())
        {
            break; // empty
        }

        if (payload.length() > 8 /* {"x":"y"} */)
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
                result = parse_game_state(response, &s_current_game);
                LOGD("%s: %d", type, result);
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
                delay(1500UL);
            }
            break;

        case CLIENT_STATE_GET_ACCOUNT:
            if (get_account())
            {
                if (stream_client.startStream("/stream/event"))
                {
                    LOGI("monitor events ok");
                    e_state = CLIENT_STATE_CHECK_EVENTS;
                }
                else
                {
                    e_state = CLIENT_STATE_IDLE;
                }
            }
            else
            {
                delay(5000UL);
                e_state = CLIENT_STATE_INIT;
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
                if (!stream_client.getEndpoint().startsWith("/api/board/game/stream"))
                {
                    stream_client.end(true);
                    delay(1000);

                    // api/board/game/stream/{gameId}
                    String endpoint = "/board/game/stream/";
                    endpoint += (const char *)s_current_game.ac_id;
                    bool b_started = stream_client.startStream(endpoint.c_str());
                    LOGI("monitor game '%s' %s", s_current_game.ac_id, b_started ? "ok" : "failed");
                }
                e_state = CLIENT_STATE_CHECK_GAME;
            }
            else
            {
                e_state = CLIENT_STATE_IDLE;
            }
            break;

        case CLIENT_STATE_CHECK_GAME:
            if (poll_game_state() < 0)
            {
                LOGD("disconnected ?");
                e_state = CLIENT_STATE_INIT;
            }
            else if (s_current_game.e_state > GAME_STATE_STARTED)
            {
                LOGD("end game stream");
                stream_client.end(true);
                memset(&s_current_game, 0, sizeof(s_current_game));
                CLEAR_BOTTOM_MENU();
                delay(1000);
                e_state = CLIENT_STATE_INIT;
            }
            else
            {
                e_state = CLIENT_STATE_IDLE;
            }
            break;

        case CLIENT_STATE_IDLE:
            if (s_current_game.ac_id[0])
            {
                if (ui::btn::pb3.getCount()) {
                    CLEAR_BOTTOM_MENU();
                    game_resign(s_current_game.ac_id);
                } else if (ui::btn::pb2.getCount()) {
                    CLEAR_BOTTOM_MENU();
                    game_abort(s_current_game.ac_id);
                }
            }
            else if (s_incoming_challenge.ac_id[0])
            {
                if (ui::btn::pb3.getCount()) {
                    CLEAR_BOTTOM_MENU();
                    decline_challenge(s_incoming_challenge.ac_id);
                } else if (ui::btn::pb2.getCount()) {
                    CLEAR_BOTTOM_MENU();
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

    stream_client.setReuse(false);
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
    bool b_status = false;
    bool b_stop   = false;

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
            b_stop = true;
        }
    }

    main_client.end(b_stop);

    return b_status;
}

bool api_post(const char *endpoint, String payload, DynamicJsonDocument &json, bool b_debug)
{
    bool b_status = false;
    bool b_stop   = false;

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
            b_stop = true;
        }

    }

    main_client.end(b_stop);

    return b_status;
}

} // namespace lichess
