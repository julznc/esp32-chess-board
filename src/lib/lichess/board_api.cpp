
#include "globals.h"
#include "apiclient.h"


namespace lichess
{

#define SAME_STR(s1, s2)            (0 == strcasecmp(s1, s2))
#define GET_NUM(o, k)               cJSON_GetNumberValue(cJSON_GetObjectItem(o, k))
#define GET_STR(o, k)               cJSON_GetStringValue(cJSON_GetObjectItem(o, k))
#define GET_STR2(o, k1, k2)         cJSON_GetStringValue(cJSON_GetObjectItem(cJSON_GetObjectItem(o, k1), k2))

static inline game_stream_state_et get_stream_state(const char *type)
{
    game_stream_state_et e_state = GAME_STREAM_STATE_UNKNOWN;

    if (SAME_STR(type, "gameFull")) {
        e_state = GAME_STREAM_STATE_FULL;
    } else if (SAME_STR(type, "gameState")) {
        e_state = GAME_STREAM_STATE_CURRENT;
    } else if (SAME_STR(type, "gameFinish")) {
        e_state = GAME_STREAM_STATE_FINISH;
    } else if (SAME_STR(type, "chatLine")) {
        e_state = GAME_STREAM_STATE_CHATLINE;
    } else if (SAME_STR(type, "opponentGone")) {
        e_state = GAME_STREAM_STATE_OPPONENTGONE;
    }

    return e_state;
}

static inline game_state_et get_state(const char *status)
{
    game_state_et e_state = GAME_STATE_UNKNOWN;

    if (SAME_STR(status, "created")) {
        e_state = GAME_STATE_CREATED;
    } else if (SAME_STR(status, "started")) {
        e_state = GAME_STATE_STARTED;
    } else if (SAME_STR(status, "aborted")) {
        e_state = GAME_STATE_ABORTED;
    } else if (SAME_STR(status, "mate")) {
        e_state = GAME_STATE_MATE;
    } else if (SAME_STR(status, "resign")) {
        e_state = GAME_STATE_RESIGN;
    } else if (SAME_STR(status, "stalemate")) {
        e_state = GAME_STATE_STALEMATE;
    } else if (SAME_STR(status, "timeout")) {
        e_state = GAME_STATE_TIMEOUT;
    } else if (SAME_STR(status, "draw")) {
        e_state = GAME_STATE_DRAW;
    } else if (SAME_STR(status, "outoftime")) {
        e_state = GAME_STATE_OUTOFTIME;
    } else if (SAME_STR(status, "cheat")) {
        e_state = GAME_STATE_CHEAT;
    } else if (SAME_STR(status, "noStart")) {
        e_state = GAME_STATE_NOSTART;
    } else if (SAME_STR(status, "unknownFinish")) {
        e_state = GAME_STATE_UNKNOWNFINISH;
    } else if (SAME_STR(status, "variantEnd")) {
        e_state = GAME_STATE_VARIANTEND;
    }

    return e_state;
}

int parse_game_event(const cJSON *event, game_st *ps_game /*output*/)
{
    const cJSON *obj = cJSON_GetObjectItem(event, "game");
    if (NULL != obj)
    {
        if (!cJSON_HasObjectItem(obj, "id") || !cJSON_HasObjectItem(obj, "status"))
        {
            LOGW("incomplete game info");
        }
        else
        {
            const char  *id     = GET_STR(obj, "id");
            const cJSON *status = cJSON_GetObjectItem(obj, "status");
            if (!cJSON_HasObjectItem(status, "id") || !cJSON_HasObjectItem(status, "name"))
            {
                LOGW("incomplete status info");
            }
            else
            {
                const char *opp_name    = GET_STR2(obj, "opponent", "username");
                const char *color       = GET_STR(obj, "color");
                const char *fen         = GET_STR(obj, "fen");
                const char *lastmove    = GET_STR(obj, "lastMove");
                const char *status_name = GET_STR(status, "name");

                strncpy(ps_game->ac_id, id, sizeof(ps_game->ac_id) - 1);
                strncpy(ps_game->ac_fen, fen, sizeof(ps_game->ac_fen) - 1);
                strncpy(ps_game->ac_opponent, opp_name, sizeof(ps_game->ac_opponent) - 1);
                strncpy(ps_game->ac_lastmove, lastmove, sizeof(ps_game->ac_lastmove) - 1);
                LOGI("%s %s", status_name, ps_game->ac_id);

                ps_game->b_color        = color[0] == 'w';
                ps_game->b_turn         = cJSON_IsTrue(cJSON_GetObjectItem(obj, "isMyTurn"));
                ps_game->e_state        = (game_state_et) cJSON_GetNumberValue(cJSON_GetObjectItem(status, "id"));

                return ps_game->e_state;
            }

        }
    }

    return GAME_STATE_UNKNOWN;
}

static inline void parse_game_state_event(const cJSON *obj, game_st *ps_game)
{
    const char *status  = GET_STR(obj, "status");
    const char *moves   = GET_STR(obj, "moves");
    ps_game->u32_wtime  = GET_NUM(obj, "wtime");
    ps_game->u32_btime  = GET_NUM(obj, "btime");
    ps_game->u32_winc   = GET_NUM(obj, "winc");
    ps_game->u32_binc   = GET_NUM(obj, "binc");

    strncpy(ps_game->ac_state, status, sizeof(ps_game->ac_state) - 1);
    strncpy(ps_game->ac_moves, moves, sizeof(ps_game->ac_moves) - 1);

    //LOGD("(%s) moves: \"%s\"", status, moves.c_str());

    ps_game->e_state = get_state(status);
}

int parse_game_state(const cJSON *state, game_st *ps_game /*output*/)
{
    if (cJSON_HasObjectItem(state, "type"))
    {
        const char *type = GET_STR(state, "type");
        //LOGD("event %s", type);

        game_stream_state_et e_type = get_stream_state(type);
        if (GAME_STREAM_STATE_FULL == e_type)
        {
            state = cJSON_GetObjectItem(state, "state");
            parse_game_state_event(state, ps_game);
        }
        else if (GAME_STREAM_STATE_CURRENT == e_type)
        {
            parse_game_state_event(state, ps_game);
        }
        else if (GAME_STREAM_STATE_CHATLINE == e_type)
        {
            LOGI("[chat] %s: %s", GET_STR(state, "username"), GET_STR(state, "text"));
        }
        else
        {
            LOGW("not yet supported stream state %d", e_type);
        }

        return ps_game->e_state;
    }
    return GAME_STATE_UNKNOWN;
}


#define SET_ENDPOINT(ep, ...)       snprintf(_uri, sizeof(_uri), ep, ## __VA_ARGS__)

// /api/board/game/{gameId}/move/{move}
bool ApiClient::game_move(const char *game_id, const char *move_uci, bool draw)
{
    SET_ENDPOINT("/api/board/game/%s/move/%s", game_id, move_uci);
    if (draw) {
        // offer (or agree to) a draw
        strcat(_uri, "?offeringDraw=true");
    }
    return api_post(_uri);
}

// api/board/game/{gameId}/abort
bool ApiClient::game_abort(const char *game_id)
{
    SET_ENDPOINT("/api/board/game/%s/abort", game_id);
    return api_post(_uri);
}

// api/board/game/{gameId}/resign
bool ApiClient::game_resign(const char *game_id)
{
    SET_ENDPOINT("/api/board/game/%s/resign", game_id);
    return api_post(_uri);
}

// api/board/game/{gameId}/draw/{accept}
bool ApiClient::handle_draw(const char *game_id, bool b_accept)
{
    SET_ENDPOINT("/api/board/game/%s/draw/%s", game_id, b_accept ? "yes" : "no");
    return api_post(_uri);
}

// api/board/game/{gameId}/takeback/{accept}
bool ApiClient::handle_takeback(const char *game_id, bool b_accept)
{
    SET_ENDPOINT("/api/board/game/%s/takeback/%s", game_id, b_accept ? "yes" : "no");
    return api_post(_uri);
}

} // namespace lichess
