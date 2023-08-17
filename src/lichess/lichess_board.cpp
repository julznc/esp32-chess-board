
#include "globals.h"
#include "lichess_api.h"


namespace lichess
{

static DynamicJsonDocument  _rsp(2*1024);


static inline game_stream_state_et get_stream_state(String &type)
{
    game_stream_state_et e_state = GAME_STREAM_STATE_UNKNOWN;

    if (type == "gameFull") {
        e_state = GAME_STREAM_STATE_FULL;
    } else if (type == "gameState") {
        e_state = GAME_STREAM_STATE_CURRENT;
    } else if (type == "gameFinish") {
        e_state = GAME_STREAM_STATE_FINISH;
    } else if (type == "chatLine") {
        e_state = GAME_STREAM_STATE_CHATLINE;
    } else if (type == "opponentGone") {
        e_state = GAME_STREAM_STATE_OPPONENTGONE;
    }

    return e_state;
}

static inline game_state_et get_state(String &status)
{
    game_state_et e_state = GAME_STATE_UNKNOWN;

    if (status == "created") {
        e_state = GAME_STATE_CREATED;
    } else if (status == "started") {
        e_state = GAME_STATE_STARTED;
    } else if (status == "aborted") {
        e_state = GAME_STATE_ABORTED;
    } else if (status == "mate") {
        e_state = GAME_STATE_MATE;
    } else if (status == "resign") {
        e_state = GAME_STATE_RESIGN;
    } else if (status == "stalemate") {
        e_state = GAME_STATE_STALEMATE;
    } else if (status == "timeout") {
        e_state = GAME_STATE_TIMEOUT;
    } else if (status == "draw") {
        e_state = GAME_STATE_DRAW;
    } else if (status == "outoftime") {
        e_state = GAME_STATE_OUTOFTIME;
    } else if (status == "cheat") {
        e_state = GAME_STATE_CHEAT;
    } else if (status == "noStart") {
        e_state = GAME_STATE_NOSTART;
    } else if (status == "unknownFinish") {
        e_state = GAME_STATE_UNKNOWNFINISH;
    } else if (status == "variantEnd") {
        e_state = GAME_STATE_VARIANTEND;
    }

    return e_state;
}

int parse_game_event(DynamicJsonDocument &json, game_st *ps_game /*output*/)
{
    if (json.containsKey("game"))
    {
        JsonObject obj = json["game"].as<JsonObject>();
        if (!obj.containsKey("id") || !obj.containsKey("status"))
        {
            LOGW("incomplete game info");
        }
        else
        {
            const char *id    = obj["id"].as<const char*>();
            JsonObject status = obj["status"].as<JsonObject>();
            if (!status.containsKey("id") || !status.containsKey("name"))
            {
                LOGW("incomplete status info");
            }
            else
            {
                JsonObject  opponent    = obj["opponent"].as<JsonObject>();
                const char *opp_name    = opponent["username"].as<const char*>();
                const char *color       = obj["color"].as<const char*>();
                const char *lastmove    = obj["lastMove"].as<const char*>();
                const char *status_name = status["name"].as<const char*>();

                strncpy(ps_game->ac_id, id, sizeof(ps_game->ac_id) - 1);
                strncpy(ps_game->ac_opponent, opp_name, sizeof(ps_game->ac_opponent) - 1);
                strncpy(ps_game->ac_lastmove, lastmove, sizeof(ps_game->ac_lastmove) - 1);
                LOGI("%s %s", status_name, ps_game->ac_id);

                ps_game->b_color        = color[0] == 'w';
                ps_game->b_turn         = obj["isMyTurn"];
                ps_game->e_state        = (game_state_et)status["id"].as<int>();

                return ps_game->e_state;
            }

        }
    }
    return GAME_STATE_UNKNOWN;
}

static inline void parse_game_state_event(JsonObject &obj, game_st *ps_game, String &moves)
{
    String status       = obj["status"].as<const char *>();

    moves               = obj["moves"].as<const char *>();
    ps_game->u32_wtime  = obj["wtime"].as<uint32_t>();
    ps_game->u32_btime  = obj["btime"].as<uint32_t>();
    ps_game->u32_winc   = obj["winc"].as<uint32_t>();
    ps_game->u32_binc   = obj["binc"].as<uint32_t>();

    //LOGD("(%s) moves: \"%s\"", status.c_str(), moves.c_str());

    ps_game->e_state = get_state(status);
}

int parse_game_state(DynamicJsonDocument &json, game_st *ps_game /*output*/, String &moves)
{
    if (json.containsKey("type"))
    {
        String type = json["type"].as<const char*>();
        //LOGD("event %s", type.c_str());

        game_stream_state_et e_type = get_stream_state(type);
        if (GAME_STREAM_STATE_FULL == e_type)
        {
            JsonObject obj = json["state"].as<JsonObject>();
            parse_game_state_event(obj, ps_game, moves);
        }
        else if (GAME_STREAM_STATE_CURRENT == e_type)
        {
            JsonObject obj = json.as<JsonObject>();
            parse_game_state_event(obj, ps_game, moves);
        }
#if 0 // this should be on "event" stream
        else if (GAME_STREAM_STATE_FINISH == e_type)
        {
            JsonObject game   = json["game"].as<JsonObject>();
            JsonObject obj    = game["status"].as<JsonObject>();
            String     status = obj["name"].as<const char*>();
            LOGI("finish: %s", status.c_str());
            ps_game->e_state = get_state(status);
        }
#endif
        else
        {
            LOGW("not yet supported stream state %d", e_type);
        }

        return ps_game->e_state;
    }
    return GAME_STATE_UNKNOWN;
}

// api/board/game/{gameId}/move/{move}
bool game_move(const char *game_id, const char *move_uci)
{
    String endpoint = "/api/board/game/";

    endpoint += game_id;
    endpoint += "/move/";
    endpoint += move_uci;

    return api_post(endpoint.c_str(), "", _rsp, true);
}

// api/board/game/{gameId}/abort
bool game_abort(const char *game_id)
{
    String endpoint = "/api/board/game/";

    endpoint += game_id;
    endpoint += "/abort";

    return api_post(endpoint.c_str(), "", _rsp, true);
}

// api/board/game/{gameId}/resign
bool game_resign(const char *game_id)
{
    String endpoint = "/api/board/game/";

    endpoint += game_id;
    endpoint += "/resign";

    return api_post(endpoint.c_str(), "", _rsp, true);
}

// api/board/game/{gameId}/draw/{accept}
bool handle_draw(const char *game_id, bool b_accept)
{
    String endpoint = "/api/board/game/";

    endpoint += game_id;
    endpoint += "/draw/";
    endpoint += b_accept ? "yes" : "no";

    return api_post(endpoint.c_str(), "", _rsp, true);
}

// api/board/game/{gameId}/takeback/{accept}
bool handle_takeback(const char *game_id, bool b_accept)
{
    String endpoint = "/api/board/game/";

    endpoint += game_id;
    endpoint += "/takeback/";
    endpoint += b_accept ? "yes" : "no";

    return api_post(endpoint.c_str(), "", _rsp, true);
}

} // namespace lichess
