
#include "globals.h"
#include "lichess_api.h"


namespace lichess
{

static DynamicJsonDocument  _rsp(2*1024);


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
                const char *status_name = status["name"].as<const char*>();
                ps_game->e_state        = (game_state_et)status["id"].as<int>();
                strncpy(ps_game->ac_id, id, sizeof(ps_game->ac_id) - 1);

                LOGI("%s %s", status_name, ps_game->ac_id);

                return ps_game->e_state;
            }

        }
    }
    return GAME_STATE_UNKNOWN;
}

int parse_game_state(DynamicJsonDocument &json, game_st *ps_game /*output*/)
{
    if (json.containsKey("type"))
    {
        const char *type = json["type"].as<const char*>();
        LOGD("event %s", type);
    }
    return GAME_STATE_UNKNOWN;
}

// api/board/game/{gameId}/move/{move}
bool game_move(const char *game_id, const char *move_uci)
{
    String endpoint = "/board/game/";

    endpoint += game_id;
    endpoint += "/move/";
    endpoint += move_uci;

    return api_post(endpoint.c_str(), "", _rsp, true);
}

// api/board/game/{gameId}/abort
bool game_abort(const char *game_id)
{
    String endpoint = "/board/game/";

    endpoint += game_id;
    endpoint += "/abort";

    return api_post(endpoint.c_str(), "", _rsp, true);
}

// api/board/game/{gameId}/resign
bool game_resign(const char *game_id)
{
    String endpoint = "/board/game/";

    endpoint += game_id;
    endpoint += "/resign";

    return api_post(endpoint.c_str(), "", _rsp, true);
}

// api/board/game/{gameId}/draw/{accept}
bool handle_draw(const char *game_id, bool b_accept)
{
    String endpoint = "/board/game/";

    endpoint += game_id;
    endpoint += "/draw/";
    endpoint += b_accept ? "yes" : "no";

    return api_post(endpoint.c_str(), "", _rsp, true);
}

// api/board/game/{gameId}/takeback/{accept}
bool handle_takeback(const char *game_id, bool b_accept)
{
    String endpoint = "/board/game/";

    endpoint += game_id;
    endpoint += "/takeback/";
    endpoint += b_accept ? "yes" : "no";

    return api_post(endpoint.c_str(), "", _rsp, true);
}

} // namespace lichess
