
#include <Preferences.h>

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
static game_st          s_current_game;
static const char      *pc_last_move = "...";
static uint32_t         ms_last_stream; // timestamp of last receive data

static char             ac_username[32];
static Preferences      configs;


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
    cJSON  *root     = NULL;
    cJSON  *item     = NULL;
    bool    b_status = false;

    SHOW_STATUS("lichess.org ...");

    if (ac_username[0])
    {
        SHOW_STATUS("%.*s", 20, ac_username);
        b_status = true;
    }
    else if (!api_get("/api/account", &root, false))
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
    char        payload_buff[1024];
    int         payload_len = 0;
    const char *endpoint = stream_client.getEndpoint();
    int         result = -1;

    if (strlen(endpoint) && (0 != strncmp(endpoint, "/api/stream/event", strlen("/api/stream/event"))))
    {
        return  0; // ignore, not for us
    }

    while (stream_client.connected())
    {
        result  = 0;
        memset(payload_buff, 0, sizeof(payload_buff));
        payload_len = stream_client.readline(payload_buff, sizeof(payload_buff) - 1);

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

static int poll_game_state()
{
    char        payload_buff[1024];
    int         payload_len = 0;
    const char *endpoint = stream_client.getEndpoint();
    int         result = -1;

    if (strlen(endpoint) && (0 != strncmp(endpoint, "/api/board/game/stream", strlen("/api/board/game/stream"))))
    {
        return  0; // ignore, not for us
    }

    while (stream_client.connected())
    {
        result  = 0;
        memset(payload_buff, 0, sizeof(payload_buff));
        payload_len = stream_client.readline(payload_buff, sizeof(payload_buff) - 1);

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
                if (sp > 0) {
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

static inline const char *get_player_name(challenge_st *ps_challenge)
{
    String opponent;
    const char *name = "";

    strncpy(ps_challenge->ac_user, "ai", sizeof(ps_challenge->ac_user));

    switch (ps_challenge->e_player)
    {
    case PLAYER_AI_LEVEL_LOW:       name = "Stockfish level 2"; break;
    case PLAYER_AI_LEVEL_MEDIUM:    name = "Stockfish level 5"; break;
    case PLAYER_AI_LEVEL_HIGH:      name = "Stockfish level 8"; break;
    case PLAYER_BOT_MAIA9:
        strncpy(ps_challenge->ac_user, "maia9", sizeof(ps_challenge->ac_user));
        name = ps_challenge->ac_user;
        break;
    case PLAYER_CUSTOM:
        opponent = configs.getString("opponent", CHALLENGE_DEFAULT_OPPONENT_NAME);
        strncpy(ps_challenge->ac_user, opponent.c_str(), sizeof(ps_challenge->ac_user));
        name = ps_challenge->ac_user;
        break;
    case PLAYER_RANDOM_SEEK:
        strncpy(ps_challenge->ac_user, "random", sizeof(ps_challenge->ac_user));
        name = "random opponent";
        break;
    }

    return name;
}

static void taskClient(void *)
{
    challenge_st            s_challenge; // outgoing challenge
    const chess::stats_st  *fen_stats   = NULL;
    char        ac_fen[FEN_BUFF_LEN] = {0, }; // current board position
    char        ac_prev_fen[FEN_BUFF_LEN] = {0, };
    char        ac_uci_move[8] = {0, };
    bool        b_opponent_changed = false;
    uint8_t     u8_error_count   = 0;

    WDT_WATCH(NULL);

    delay(2500UL);

    s_challenge.e_player            = CHALLENGE_DEFAULT_OPPONENT;
    s_challenge.b_rated             = configs.getBool("rated", false);
    s_challenge.u16_clock_limit     = configs.getUShort("clock_limit", CHALLENGE_DEFAULT_LIMIT);
    s_challenge.u8_clock_increment  = configs.getUChar("clock_increment", CHALLENGE_DEFAULT_INCREMENT);

    for (;;)
    {
        WDT_FEED();

        switch (e_state)
        {
        case CLIENT_STATE_INIT:
            if ((u8_error_count < 10) && wifi::connected())
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
                u8_error_count++;
                e_state = CLIENT_STATE_INIT;
                delay(5000UL);
            }
            break;

        case CLIENT_STATE_CHECK_EVENTS:
            if (s_current_game.ac_id[0])
            {
                if (0 != strncmp(stream_client.getEndpoint(), "/api/board/game/stream", strlen("/api/board/game/stream")))
                {
                    stream_client.end(true);
                    delay(100);

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
              #if 0
                delay(1000);
                e_state = CLIENT_STATE_INIT;
              #elif 1 // halt task
                SET_BOTTOM_MENU("            Restart->");
                while (1) {
                    WDT_FEED();
                    if (RIGHT_BTN.shortPressed()) {
                        ESP.restart();
                    }
                    delay(1000);
                }
              #endif
            }
            else
            {
                e_state = CLIENT_STATE_CHECK_BOARD;
            }
            break;

        case CLIENT_STATE_CHECK_BOARD:
            if (!ac_fen[0] && !s_current_game.ac_id[0]) // if not yet started
            {
                if (chess::get_position(ac_fen))
                {
                    if (0 != strcmp(ac_fen, START_FEN)) {
                        s_challenge.e_player = PLAYER_AI_LEVEL_HIGH;
                    }
                    SHOW_OPPONENT(get_player_name(&s_challenge));
                    SET_BOTTOM_MSG ("Play from position ?");
                    SET_BOTTOM_MENU("<-Black       White->");
                }
            }
            else if (GAME_STATE_STARTED == s_current_game.e_state) // if has on-going game
            {
                fen_stats = chess::get_position(ac_fen, ac_uci_move);
                bool b_turn = (NULL != strchr(ac_fen, 'w'));

                if (0 != strncmp(ac_prev_fen, ac_fen, sizeof(ac_prev_fen)))
                {
                    if ((0 != ac_uci_move[0]) && (b_turn != s_current_game.b_color))
                    {
                        if (game_move(s_current_game.ac_id, ac_uci_move))
                        {
                            LOGD("send move %s ok", ac_uci_move);
                            strncpy(ac_prev_fen, ac_fen, sizeof(ac_prev_fen));
                        }
                    }
                }

                display_clock(b_turn, true);
            }
            else if (s_current_game.e_state > GAME_STATE_STARTED)
            {
                // finished
                memset(ac_fen, 0, sizeof(ac_fen));
                memset(ac_prev_fen, 0, sizeof(ac_prev_fen));
                memset(ac_uci_move, 0, sizeof(ac_uci_move));
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
            else if (ac_fen[0])
            {
                if (RIGHT_BTN.pressedDuration() >= 1200UL) {
                    RIGHT_BTN.resetCount();
                    if (s_challenge.e_player < PLAYER_LAST_IDX) {
                        s_challenge.e_player++;
                        if ((s_challenge.e_player > PLAYER_AI_LEVEL_HIGH) && (0 != strcmp(ac_fen, START_FEN))) {
                            // allow custom position on AI opponent only
                            s_challenge.e_player = PLAYER_AI_LEVEL_HIGH;
                        }
                    }
                    SHOW_OPPONENT(get_player_name(&s_challenge));
                    b_opponent_changed = true;
                }
                else if (LEFT_BTN.pressedDuration() >= 1200UL) {
                    LEFT_BTN.resetCount();
                    if (s_challenge.e_player > PLAYER_FIRST_IDX) {
                        s_challenge.e_player--;
                    }
                    SHOW_OPPONENT(get_player_name(&s_challenge));
                    b_opponent_changed = true;
                }
                else if (b_opponent_changed) {
                    delay(2000UL);
                    b_opponent_changed = false;
                }
                else if (RIGHT_BTN.shortPressed()) {
                    CLEAR_BOTTOM_MENU();
                    SET_BOTTOM_MSG("wait for opponent...");
                    s_challenge.b_color = true;
                    if (!create_challenge(&s_challenge, ac_fen)) {
                        SET_BOTTOM_MSG("              Retry->");
                    }
                }
                else if (LEFT_BTN.shortPressed()) {
                    CLEAR_BOTTOM_MENU();
                    SET_BOTTOM_MSG("wait for opponent...");
                    s_challenge.b_color = false;
                    if (!create_challenge(&s_challenge, ac_fen)) {
                        SET_BOTTOM_MSG("<-Retry");
                    }
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

    if (!configs.begin("lichess", false))
    {
        LOGE("load configs failed");
    }
    else
    {
        xTaskCreate(
            taskClient,     /* Task function. */
            "taskClient",   /* String with name of task. */
            32*1024,        /* Stack size in bytes. */
            NULL,           /* Parameter passed as input of the task */
            4,              /* Priority of the task. */
            NULL);          /* Task handle. */
    }
}

bool get_username(String &name)
{
    if (e_state > CLIENT_STATE_GET_ACCOUNT)
    {
        name = ac_username;
        return true;
    }
    return false;
}

size_t set_token(const char *token)
{
    size_t len = configs.putString("token", token);
    LOGI("set(%s) = %lu", token, len);
    return len;
}

bool get_token(String &token)
{
    token = configs.getString("token");
    //LOGD("token = %s", token.c_str());
    return token.length() > 0;
}

bool get_token(char *token, size_t len)
{
    return configs.getString("token", token, len) > 0;
}

bool set_game_options(String &opponent, uint16_t u16_clock_limit, uint8_t u8_clock_increment, bool b_rated)
{
    if ((configs.putString("opponent", opponent) < opponent.length()) ||
        (configs.putUShort("clock_limit", u16_clock_limit) < sizeof(uint16_t)) ||
        (configs.putUChar("clock_increment", u8_clock_increment) < sizeof(uint8_t)) ||
        (configs.putBool("rated", b_rated) < sizeof(bool)))
    {
        LOGW("failed");
        return false;
    }

    return true;
}

bool get_game_options(String &opponent, uint16_t &u16_clock_limit, uint8_t &u8_clock_increment, bool &b_rated)
{
    // default to maia9 bot
    opponent = configs.getString("opponent", CHALLENGE_DEFAULT_OPPONENT_NAME);
    // default to 15+10
    u16_clock_limit = configs.getUShort("clock_limit", CHALLENGE_DEFAULT_LIMIT);
    u8_clock_increment = configs.getUChar("clock_increment", CHALLENGE_DEFAULT_INCREMENT);
    // default to casual
    b_rated = configs.getBool("rated", false);

    return true;
}

} // namespace lichess
