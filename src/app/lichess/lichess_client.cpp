
#include "globals.h"
#include "chess/chess.h"
#include "ui/ui.h"
#include "wifi/wifi_setup.h"

#include "lichess_client.h"


#define SHOW_STATUS(msg, ...)       DISPLAY_CLEAR_ROW(10, 8); \
                                    DISPLAY_TEXT1(0, 10, msg, ## __VA_ARGS__)

#define SHOW_OPPONENT(name, ...)    DISPLAY_CLEAR_ROW(32, 8); \
                                    DISPLAY_TEXT1(0, 32, name, ## __VA_ARGS__)

#define SET_BOTTOM_MSG(msg, ...)    DISPLAY_CLEAR_ROW(45, 9); \
                                    DISPLAY_TEXT1(0, 45, msg, ## __VA_ARGS__)

#define SET_BOTTOM_MENU(msg, ...)   DISPLAY_CLEAR_ROW(54, 9); \
                                    DISPLAY_TEXT1(0, 54, msg, ## __VA_ARGS__)

#define CLEAR_BOTTOM_MENU()         DISPLAY_CLEAR_ROW(45, 18);

namespace lichess
{

static ApiClient        main_client;    // main connection (non-stream'ed)
static ApiClient        stream_client;
static challenge_st     s_incoming_challenge;
static game_st          s_current_game;
static const char      *pc_last_move = "...";

static char             ac_username[32];
static char             payload_buff[2048];
static uint32_t         ms_last_stream; // timestamp of last receive data
static uint8_t          u8_error_count;

static enum {
    CLIENT_STATE_INIT,
    CLIENT_STATE_GET_ACCOUNT,   // check lichess account/profile
    CLIENT_STATE_CHECK_EVENTS,  // check messages from eventd-stream
    CLIENT_STATE_CHECK_GAME,    // check messages from game-state stream
    CLIENT_STATE_CHECK_BOARD,   // get board status
    CLIENT_STATE_GAME_FINISHED, // get board status
    CLIENT_STATE_IDLE
} e_state;

static struct {
    nvs_handle_t    nvs;
    char            ac_token[64]; // api token
    // challenge
    char            ac_opponent[64];
    uint16_t        u16_clock_limit;
    uint8_t         u8_clock_increment;
    uint8_t         b_rated;
} s_configs;


static bool get_account();
static int poll_events();
static int poll_game_state();
static void display_clock(bool b_turn, bool b_show);


bool init()
{
    //LOGD("%s()", __func__);

    SecClient::lib_init();

    memset(&ac_username, 0, sizeof(ac_username));
    memset(&s_configs, 0, sizeof(s_configs));

    if (ESP_OK != nvs_open("lichess", NVS_READWRITE, &s_configs.nvs))
    {
        LOGW("open configs failed");
        s_configs.nvs = 0;
    }
    else
    {
#if 0 // initial test
        set_token("my_Token");
#endif
        if (get_token(NULL)) {
            ApiClient::lib_init(s_configs.ac_token);
        }

    }

    u8_error_count = 0;
    e_state = CLIENT_STATE_INIT;

    return true;
}

void loop()
{
    switch (e_state)
    {
    case CLIENT_STATE_INIT:
        if ((u8_error_count < 10) && wifi::connected())
        {
            if (!ac_username[0]) {
                delayms(2500); // initial delay
            }
            e_state = CLIENT_STATE_GET_ACCOUNT;
        }
        else
        {
            delayms(1500UL);
        }
        break;

    case CLIENT_STATE_GET_ACCOUNT:
        if (get_account())
        {
            if (stream_client.startStream("/api/stream/event"))
            {
                LOGI("monitor events ok");
                ms_last_stream = millis();
                u8_error_count = 0;
                e_state = CLIENT_STATE_CHECK_EVENTS;
            }
            else
            {
                LOGW("monitor events failed");
                u8_error_count++;
                e_state = CLIENT_STATE_IDLE;
            }
        }
        else
        {
            delayms(2500);
            u8_error_count++;
            e_state = CLIENT_STATE_INIT;
        }
        break;

    case CLIENT_STATE_CHECK_EVENTS:
        if (s_current_game.ac_id[0])
        {
            if (0 != strncmp(stream_client.getEndpoint(), "/api/board/game/stream", strlen("/api/board/game/stream")))
            {
                stream_client.end(true);
                delayms(100);

                char endpoint[64]; // /api/board/game/stream/{gameId}
                snprintf(endpoint, sizeof(endpoint), "/api/board/game/stream/%s", s_current_game.ac_id);

                SHOW_STATUS("stream game...");
                bool b_started = stream_client.startStream(endpoint);

                if (!b_started)
                {
                    LOGW("monitor game '%s' failed", s_current_game.ac_id);
                    SHOW_STATUS("stream game failed");
                    if (++u8_error_count >= 3) {
                        e_state = CLIENT_STATE_INIT;
                    }
                    break;
                }

                ms_last_stream = millis();
                u8_error_count = 0;
                LOGI("monitor game '%s' ok", s_current_game.ac_id);
                SHOW_STATUS("game: %.*s", 14, s_current_game.ac_id);
            }
            e_state = CLIENT_STATE_CHECK_GAME;
        }
        else if (poll_events() < 0)
        {
            e_state = CLIENT_STATE_INIT;
        }
        else
        {
            e_state = CLIENT_STATE_CHECK_BOARD;
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
            SHOW_OPPONENT("finish: %s", s_current_game.ac_state);
            stream_client.end(true);
            memset(&s_current_game, 0, sizeof(s_current_game));
            CLEAR_BOTTOM_MENU();
            // halt task
            SET_BOTTOM_MENU("            Restart->");
            e_state = CLIENT_STATE_GAME_FINISHED;
        }
        else
        {
            e_state = CLIENT_STATE_CHECK_BOARD;
        }
        break;

    case CLIENT_STATE_CHECK_BOARD:
        break;

    case CLIENT_STATE_GAME_FINISHED:
        if (RIGHT_BTN.shortPressed())
        {
            esp_restart();
        }
        delayms(100);
        break;

    case CLIENT_STATE_IDLE:
        break;

    default:
        e_state = CLIENT_STATE_INIT;
        break;
    }
}

bool get_token(const char **token)
{
    esp_err_t   err;
    size_t      len = sizeof(s_configs.ac_token);
    bool        b_status = false;

    if (ESP_OK != (err = nvs_get_str(s_configs.nvs, "token", s_configs.ac_token, &len)))
    {
        LOGW("nvs_get_str() = %d", err);
    }
    else
    {
        if (NULL != token)
        {
            *token = s_configs.ac_token;
        }
        //LOGD("token (%u): \"%s\"", len, s_configs.ac_token);
        b_status = (0 != s_configs.ac_token[0]);
    }

    return b_status;
}

bool set_token(const char *token)
{
    esp_err_t   err;
    bool        b_status = false;

    if (NULL == token)
    {
        //
    }
    else if (ESP_OK != (err = nvs_set_str(s_configs.nvs, "token", token)))
    {
        LOGW("nvs_set_str() = %d", err);
    }
    else if (ESP_OK != (err = nvs_commit(s_configs.nvs)))
    {
        LOGW("nvs_commit() = %d", err);
    }
    else
    {
        b_status = true;
    }

    return b_status;
}

static bool get_account()
{
    cJSON  *root     = NULL;
    cJSON  *item     = NULL;
    bool    b_status = false;

    SHOW_STATUS("lichess.org ...");

    if (ac_username[0])
    {
        SHOW_STATUS("%.*s", 20, ac_username);
        b_status = true;
    }
    else if (!main_client.api_get("/api/account", &root, false))
    {
        SHOW_STATUS("lichess api error");
    }
    else if (NULL == (item = cJSON_GetObjectItem(root, "username")))
    {
        LOGW("unknown username");
        SHOW_STATUS("unknown username");
    }
    else
    {
        const char *username = cJSON_GetStringValue(item);
        LOGI("username : %s", username);
        strncpy(ac_username, username, sizeof(ac_username) - 1);
        SHOW_STATUS("%.*s", 20, ac_username);
        b_status = true;
    }

    if (root)
    {
        cJSON_Delete(root);
    }

    return b_status;
}

static int poll_events()
{
    const char *endpoint = stream_client.getEndpoint();
    int         payload_len = 0;
    int         result = -1;

    if (strlen(endpoint) && (0 != strncmp(endpoint, "/api/stream/event", strlen("/api/stream/event"))))
    {
        return  0; // ignore, not for us
    }

    while (stream_client.connected())
    {
        result  = 0;
        memset(payload_buff, 0, sizeof(payload_buff));
        payload_len = stream_client.readline(payload_buff, sizeof(payload_buff) - 1, 10);

        if (payload_len < 1)
        {
            if (millis() - ms_last_stream > (6000UL + 500)) // should receive every 6 seconds
            {
                LOGW("no data received (%lums)", millis() - ms_last_stream);
                stream_client.end(true);
                result = -1;
            }
            break; // empty
        }

        ms_last_stream = millis();

        if (payload_len > 8 /* {"x":"y"} */)
        {
            LOGD("payload (%d): %s", payload_len, payload_buff);
            cJSON *response = NULL;
            cJSON *objtype  = NULL;
            if (NULL == (response = cJSON_Parse(payload_buff)))
            {
                LOGW("parse json failed (%s)", cJSON_GetErrorPtr());
            }
            else if (NULL == (objtype = cJSON_GetObjectItem(response, "type")))
            {
                LOGW("unknown event type");
            }
            else
            {
                const char *type = cJSON_GetStringValue(objtype);
                //LOGD("event %s", type);
                if (0 == strncmp(type, "game", strlen("game")))
                {
                    result = parse_game_event(response, &s_current_game);
                    DISPLAY_CLEAR_ROW(45, SCREEN_HEIGHT-45);
                    if ((GAME_STATE_STARTED == result) && s_current_game.ac_id[0])
                    {
                        if (0 == s_current_game.ac_moves[0]) {
                            chess::continue_game(s_current_game.ac_fen);
                        }
                        memset(s_current_game.ac_moves, 0, sizeof(s_current_game.ac_moves)); // clear starting moves
                        SHOW_OPPONENT("%.17s %c", s_current_game.ac_opponent, s_current_game.b_color ? 'B' : 'W');
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
                            SET_BOTTOM_MSG("%.*s(%c %u+%u)", 12, pc->ac_user, pc->b_color ? 'B' : 'W',
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

            if (NULL != response)
            {
                cJSON_Delete(response);
            }
        }
        delayms(20);
    }

    return result;
}

static int poll_game_state()
{
    const char *endpoint = stream_client.getEndpoint();
    int         payload_len = 0;
    int         result = -1;

    if (strlen(endpoint) && (0 != strncmp(endpoint, "/api/board/game/stream", strlen("/api/board/game/stream"))))
    {
        return  0; // ignore, not for us
    }

    while (stream_client.connected())
    {
        result  = 0;
        memset(payload_buff, 0, sizeof(payload_buff));
        payload_len = stream_client.readline(payload_buff, sizeof(payload_buff) - 1, 10);

        if (payload_len < 1)
        {
            if (millis() - ms_last_stream > (6000UL + 500)) // should receive every 6 seconds
            {
                LOGW("no data received (%lums)", millis() - ms_last_stream);
                stream_client.end(true);
                result = -1;
            }
            break; // empty
        }

        ms_last_stream = millis();

        if (payload_len > 8 /* {"x":"y"} */)
        {
            //LOGD("payload (%d): %s", payload_len, payload_buff);
            cJSON *response = NULL;
            cJSON *objtype  = NULL;
            if (NULL == (response = cJSON_Parse(payload_buff)))
            {
                LOGW("parse json failed (%s)", cJSON_GetErrorPtr());
            }
            else if (NULL == (objtype = cJSON_GetObjectItem(response, "type")))
            {
                LOGW("unknown game state");
                display_clock(false, false);
            }
            else
            {
                const char *type = cJSON_GetStringValue(objtype);
                result = parse_game_state(response, &s_current_game);
                pc_last_move = s_current_game.ac_moves;
                const char *sp = strrchr(pc_last_move, ' ');
                if (NULL != sp) {
                    pc_last_move = sp + 1;
                }
                LOGD("%s (%d) %s", type, result, pc_last_move);
                chess::queue_move(pc_last_move);
                //LOGD("%s", s_current_game.ac_moves);
                display_clock(s_current_game.b_turn, true);
            }

            if (NULL != response)
            {
                cJSON_Delete(response);
            }
        }

    }

    return result;
}

static void display_clock(bool b_turn, bool b_show)
{
    static uint32_t ms_last_update = millis();
    uint32_t ms_now = millis();
    uint32_t ms_diff = ms_now - ms_last_update;

    if (!b_show)
    {
        DISPLAY_CLEAR_ROW(45, 9);
    }
    else
    {
        uint32_t wsecs = s_current_game.u32_wtime / 1000UL;
        uint32_t bsecs = s_current_game.u32_btime / 1000UL;
        uint32_t wmins = wsecs / 60;
        uint32_t bmins = bsecs / 60;

        wsecs = wsecs % 60;
        bsecs = bsecs % 60;
        //" 000:00       000:00 "
        //DISPLAY_TEXT1(0, 45, " %3u:%02u       %3u:%02u ", bmins, bsecs, wmins, wsecs);
        DISPLAY_TEXT1(0, 45, "%3u:%02u  %-5s %3u:%02u ", bmins, bsecs, pc_last_move, wmins, wsecs);
    }

    if (b_turn) {
        s_current_game.u32_wtime -= ms_diff;
    } else {
        s_current_game.u32_btime -= ms_diff;
    }
    ms_last_update = ms_now;
}

} // namespace lichess
