
#include "globals.h"
#include "lichess_api.h"


namespace lichess
{


int parse_challenge_event(DynamicJsonDocument &json, challenge_st *ps_challenge)
{
    if (json.containsKey("challenge"))
    {
        JsonObject obj = json["challenge"].as<JsonObject>();
        if (!obj.containsKey("id")          || !obj.containsKey("status")   ||
            !obj.containsKey("challenger")  || !obj.containsKey("destUser") ||
            !obj.containsKey("variant")     || !obj.containsKey("rated")    ||
            !obj.containsKey("speed")       || !obj.containsKey("timeControl"))
        {
            LOGW("incomplete info");
        }
        else
        {
            const char *id     = obj["id"].as<const char*>();
            const char *status = obj["status"].as<const char*>();
            LOGI("%s: %s", status, id);
        }
    }

    return CHALLENGE_NONE;
}

} // namespace lichess