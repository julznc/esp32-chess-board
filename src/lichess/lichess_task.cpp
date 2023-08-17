
#include "globals.h"

#include "chess/chess.h"
#include "ui/ui.h"
#include "wifi/wifi_setup.h"

#include "lichess_api.h"


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

static ApiClient        stream_client;  // both events-stream & board-state-stream connections
static challenge_st     s_incoming_challenge;
static String           str_current_moves;
static String           str_last_move;
static game_st          s_current_game;

static char             ac_username[32];
DynamicJsonDocument     response(2*1024);


static enum {
    CLIENT_STATE_INIT,
    CLIENT_STATE_GET_ACCOUNT,   // check lichess account/profile
    CLIENT_STATE_CHECK_EVENTS,  // check messages from eventd-stream
    CLIENT_STATE_CHECK_GAME,    // check messages from game-state stream
    CLIENT_STATE_CHECK_BOARD,   // get board status
    CLIENT_STATE_IDLE
} e_state;


static bool get_account()
{
    bool b_status = false;

    SHOW_STATUS("lichess.org ...");

    memset(ac_username, 0, sizeof(ac_username));

    if (!api_get("/api/account", response))
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
        payload = stream_client.readLine();
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
                        str_current_moves = ""; // clear starting moves
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
        }
        delay(100);
    }

    return result;
}

static inline void display_clock(bool b_turn, bool b_show)
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
        DISPLAY_TEXT1(0, 45, " %3u:%02u       %3u:%02u ", bmins, bsecs, wmins, wsecs);
    }

    if (b_turn) {
        s_current_game.u32_wtime -= ms_diff;
    } else {
        s_current_game.u32_btime -= ms_diff;
    }
    ms_last_update = ms_now;
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
        payload = stream_client.readLine();
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
                display_clock(false, false);
            }
            else
            {
                const char *type = response["type"].as<const char*>();
                result = parse_game_state(response, &s_current_game, str_current_moves);
                str_last_move = str_current_moves;
                int sp = str_current_moves.length() ? str_current_moves.lastIndexOf(' ') : 0;
                if (sp > 0) {
                    str_last_move = str_current_moves.substring(sp + 1);
                }
                LOGD("%s (%d) %s", type, result, str_last_move.c_str());
                //LOGD("%s", str_current_moves.c_str());
                display_clock(s_current_game.b_turn, true);
            }
        }

    }

    return result;
}

static void taskClient(void *)
{
    String          fen, prev_fen;
    String          last_move, queue_move;
    challenge_st    s_challenge; // outgoing challenge
    const chess::stats_st *fen_stats = NULL;
    bool            b_level_adjusted = false;

    WDT_WATCH(NULL);

    delay(2500UL);

    strncpy(s_challenge.ac_user, "ai", sizeof(s_challenge.ac_user) - 1);
    s_challenge.u8_level            = 1;
    s_challenge.b_rated             = false;
    s_challenge.u16_clock_limit     = 65 * 60;
    s_challenge.u8_clock_increment  = 30;

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
                if (stream_client.startStream("/api/stream/event"))
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
                    String endpoint = "/api/board/game/stream/";
                    endpoint += (const char *)s_current_game.ac_id;
                    bool b_started = stream_client.startStream(endpoint.c_str());
                    LOGI("monitor game '%s' %s", s_current_game.ac_id, b_started ? "ok" : "failed");
                }
                e_state = CLIENT_STATE_CHECK_GAME;
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
                stream_client.end(true);
                memset(&s_current_game, 0, sizeof(s_current_game));
                CLEAR_BOTTOM_MENU();
                delay(1000);
                e_state = CLIENT_STATE_INIT;
            }
            else
            {
                e_state = CLIENT_STATE_CHECK_BOARD;
            }
            break;

        case CLIENT_STATE_CHECK_BOARD:
            if (fen.isEmpty()) // if not yet started
            {
                if (chess::get_position(fen))
                {
                    SET_BOTTOM_MSG ("Play from position ?");
                    SET_BOTTOM_MENU("<-Black       White->");
                }
            }
            else if (GAME_STATE_STARTED == s_current_game.e_state) // if has on-going game
            {
                fen_stats = chess::get_position(fen, last_move);

                if (prev_fen != fen)
                {
                    if (!last_move.isEmpty() && (last_move != queue_move) && (fen_stats->turn != s_current_game.b_color))
                    {
                        if (game_move(s_current_game.ac_id, last_move.c_str()))
                        {
                            LOGD("send move %s ok", last_move.c_str());
                            prev_fen = fen;
                        }
                    }
                }

                if (!str_last_move.isEmpty() && (str_last_move != queue_move) && (fen_stats->turn != s_current_game.b_color))
                {
                    //LOGD("need queue %s? turn %d - %d", str_last_move.c_str(), fen_stats->turn, s_current_game.b_color);
                    queue_move = str_last_move;
                    chess::queue_move(queue_move);
                }

                display_clock(s_current_game.b_turn, true);
            }
            else if (s_current_game.e_state > GAME_STATE_STARTED)
            {
                // finished
                fen = "";
                prev_fen = "";
                last_move = "";
                queue_move = "";
            }
            e_state = CLIENT_STATE_IDLE;
            break;

        case CLIENT_STATE_IDLE:
            if (GAME_STATE_STARTED == s_current_game.e_state)
            {
                if (RIGHT_BTN.getCount()) {
                    CLEAR_BOTTOM_MENU();
                    game_resign(s_current_game.ac_id);
                } else if (LEFT_BTN.getCount()) {
                    CLEAR_BOTTOM_MENU();
                    game_abort(s_current_game.ac_id);
                }
            }
            else if (s_incoming_challenge.ac_id[0])
            {
                if (RIGHT_BTN.getCount()) {
                    CLEAR_BOTTOM_MENU();
                    decline_challenge(s_incoming_challenge.ac_id);
                } else if (LEFT_BTN.getCount()) {
                    CLEAR_BOTTOM_MENU();
                    accept_challenge(s_incoming_challenge.ac_id);
                }
            }
            else if (!fen.isEmpty())
            {
                //
                if (RIGHT_BTN.pressedDuration() >= 1500UL) {
                    RIGHT_BTN.resetCount();
                    if (s_challenge.u8_level < 8) {
                        s_challenge.u8_level++;
                    }
                    SHOW_OPPONENT("Stockfish level %u", s_challenge.u8_level);
                    b_level_adjusted = true;
                }
                else if (LEFT_BTN.pressedDuration() >= 1500UL) {
                    LEFT_BTN.resetCount();
                    if (s_challenge.u8_level > 1) {
                        s_challenge.u8_level--;
                    }
                    SHOW_OPPONENT("Stockfish level %u", s_challenge.u8_level);
                    b_level_adjusted = true;
                }
                else if (b_level_adjusted) {
                    delay(1500UL);
                    b_level_adjusted = false;
                }
                else if (RIGHT_BTN.shortPressed()) {
                    CLEAR_BOTTOM_MENU();
                    s_challenge.b_color = true;
                    create_challenge(&s_challenge, fen.c_str());
                }
                else if (LEFT_BTN.shortPressed()) {
                    CLEAR_BOTTOM_MENU();
                    s_challenge.b_color = false;
                    create_challenge(&s_challenge, fen.c_str());
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

    xTaskCreate(
        taskClient,     /* Task function. */
        "taskClient",   /* String with name of task. */
        32*1024,        /* Stack size in bytes. */
        NULL,           /* Parameter passed as input of the task */
        4,              /* Priority of the task. */
        NULL);          /* Task handle. */
}

} // namespace lichess
