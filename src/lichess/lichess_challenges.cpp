
#include "globals.h"
#include "lichess_api.h"


namespace lichess
{

static DynamicJsonDocument  _rsp(2*1024);


static inline challenge_type_et get_type(String &status)
{
    challenge_type_et e_type = CHALLENGE_NONE;

    if (status == "created") {
        e_type = CHALLENGE_CREATED;
    } else if (status == "accepted") {
        e_type = CHALLENGE_ACCEPTED;
    } else if (status == "canceled") {
        e_type = CHALLENGE_CANCELED;
    } else if (status == "declined") {
        e_type = CHALLENGE_DECLINED;
    }

    return e_type;
}

static inline game_variant_et get_variant(String &variant)
{
    game_variant_et e_variant = VARIANT_UNKNOWN;

    if (variant == "standard") {
        e_variant = VARIANT_STANDARD;
    } else if (variant == "chess960") {
        //e_variant = VARIANT_CHESS960; to do
    }

    return e_variant;
}

static inline game_speed_et get_speed(String &speed)
{
    game_speed_et e_speed = SPEED_UNKNOWN;

    if (speed == "blitz") {
        e_speed = SPEED_BLITZ;
    } else if (speed == "rapid") {
        e_speed = SPEED_RAPID;
    } else if (speed == "classical") {
        e_speed = SPEED_CLASSICAL;
    }

    return e_speed;
}

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
            const  char *id     = obj["id"].as<const char*>();
            bool   b_rated      = obj["rated"];
            String status       = obj["status"].as<const char*>();
            String challenger   = obj["challenger"].as<JsonObject>()["name"];
            String destUser     = obj["destUser"].as<JsonObject>()["name"];
            String variant      = obj["variant"].as<JsonObject>()["name"];
            String speed        = obj["speed"].as<const char*>();
            String color        = obj["color"].as<const char*>();
            JsonObject timectl  = obj["timeControl"].as<JsonObject>();

            if (color == "random") {
                color = obj["finalColor"].as<const char*>();
            }

            //LOGD("%s %s by %s to %s", status.c_str(), id, challenger.c_str(), destUser.c_str());

            challenge_type_et e_type = get_type(status);

            if (CHALLENGE_CREATED == e_type)
            {
                strncpy(ps_challenge->ac_id, id, sizeof(ps_challenge->ac_id));
                strncpy(ps_challenge->ac_user, challenger.c_str(), sizeof(ps_challenge->ac_user));
                ps_challenge->e_variant = get_variant(variant);
                ps_challenge->e_speed   = get_speed(variant);
                ps_challenge->b_rated   = b_rated;
                ps_challenge->b_color   = (color == "white");
                ps_challenge->u16_clock_limit    = timectl["limit"].as<uint16_t>();
                ps_challenge->u8_clock_increment = timectl["increment"].as<uint8_t>();
            }

            return e_type;
        }
    }

    return CHALLENGE_NONE;
}

bool accept_challenge(const char *challengeId)
{
    LOGD("%s(%s)", __func__, challengeId);
    // to do
    delay(2000);
    return false;
}

bool decline_challenge(const char *challengeId, const char *reason)
{
    String endpoint = "/challenge/";
    String payload  = "reason=";
    //LOGD("%s(%s)", __func__, challengeId);

    endpoint += challengeId;
    endpoint += "/decline";
    payload  += reason ? reason : "generic";

    return api_post(endpoint.c_str(), payload, _rsp, true);
}

bool cancel_challenge(const char *challengeId)
{
    LOGD("%s(%s)", __func__, challengeId);
    // to do
    delay(2000);
    return false;
}

} // namespace lichess