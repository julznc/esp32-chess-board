
#include "globals.h"
#include "apiclient.h"


namespace lichess
{

#define SAME_STR(s1, s2)            (0 == strcasecmp(s1, s2))
#define GET_NUM(o, k)               cJSON_GetNumberValue(cJSON_GetObjectItem(o, k))
#define GET_BOOL(o, k)              cJSON_IsTrue(cJSON_GetObjectItem(o, k))
#define GET_STR(o, k)               cJSON_GetStringValue(cJSON_GetObjectItem(o, k))
#define GET_STR2(o, k1, k2)         cJSON_GetStringValue(cJSON_GetObjectItem(cJSON_GetObjectItem(o, k1), k2))


static inline challenge_type_et get_type(const char *status)
{
    challenge_type_et e_type = CHALLENGE_NONE;

    if (SAME_STR(status, "created")) {
        e_type = CHALLENGE_CREATED;
    } else if (SAME_STR(status,  "accepted")) {
        e_type = CHALLENGE_ACCEPTED;
    } else if (SAME_STR(status, "canceled")) {
        e_type = CHALLENGE_CANCELED;
    } else if (SAME_STR(status, "declined")) {
        e_type = CHALLENGE_DECLINED;
    }

    return e_type;
}

static inline game_variant_et get_variant(const char *variant)
{
    game_variant_et e_variant = VARIANT_UNKNOWN;

    if (SAME_STR(variant, "standard")) {
        e_variant = VARIANT_STANDARD;
    } else if (SAME_STR(variant, "chess960")) {
        //e_variant = VARIANT_CHESS960; to do
    }

    return e_variant;
}

static inline game_speed_et get_speed(const char *speed)
{
    game_speed_et e_speed = SPEED_UNKNOWN;

    if (SAME_STR(speed, "blitz")) {
        e_speed = SPEED_BLITZ;
    } else if (SAME_STR(speed, "rapid")) {
        e_speed = SPEED_RAPID;
    } else if (SAME_STR(speed, "classical")) {
        e_speed = SPEED_CLASSICAL;
    }

    return e_speed;
}

int parse_challenge_event(const cJSON *event, challenge_st *ps_challenge)
{
    const cJSON *obj = cJSON_GetObjectItem(event, "challenge");
    if (NULL != obj)
    {
        if (!cJSON_HasObjectItem(obj, "id")         || !cJSON_HasObjectItem(obj, "status")   ||
            !cJSON_HasObjectItem(obj, "challenger") || !cJSON_HasObjectItem(obj, "destUser") ||
            !cJSON_HasObjectItem(obj, "variant")    || !cJSON_HasObjectItem(obj, "rated")    ||
            !cJSON_HasObjectItem(obj, "speed")      || !cJSON_HasObjectItem(obj, "timeControl"))
        {
            LOGW("incomplete info");
        }
        else
        {
            const char *id          = GET_STR(obj, "id");
            bool   b_rated          = GET_BOOL(obj, "rated");
            const char *status      = GET_STR(obj, "status");
            const char *challenger  = GET_STR2(obj, "challenger", "name");
            const char *destUser    = GET_STR2(obj, "destUser", "name");
            const char *variant     = GET_STR2(obj, "variant", "name");
          //const char *speed       = GET_STR(obj, "speed");
            const char *color       = GET_STR(obj, "color");
            const cJSON *timectl    = cJSON_GetObjectItem(obj, "timeControl");

            if (color && SAME_STR(color, "random")) {
                color = GET_STR(obj, "finalColor");
            }

            LOGD("%s %s by %s to %s", status, id, challenger, destUser);

            challenge_type_et e_type = get_type(status);

            if (CHALLENGE_CREATED == e_type)
            {
                strncpy(ps_challenge->ac_id, id, sizeof(ps_challenge->ac_id) - 1);
                strncpy(ps_challenge->ac_user, challenger, sizeof(ps_challenge->ac_user) - 1);
                ps_challenge->e_variant = get_variant(variant);
                ps_challenge->e_speed   = get_speed(variant);
                ps_challenge->b_rated   = b_rated;
                ps_challenge->b_color   = 'w' == color[0];
                ps_challenge->u16_clock_limit    = GET_NUM(timectl, "limit");
                ps_challenge->u8_clock_increment = GET_NUM(timectl, "increment");
            }

            return e_type;
        }
    }

    return CHALLENGE_NONE;
}


#define SET_ENDPOINT(ep, ...)       snprintf(_uri, sizeof(_uri), ep, ## __VA_ARGS__)
#define SET_PAYLOAD(pl, ...)        snprintf(_rsp_buf, sizeof(_rsp_buf), pl, ## __VA_ARGS__)

bool ApiClient::accept_challenge(const char *challenge_id)
{
    //LOGD("%s(%s)", __func__, challenge_id);

    SET_ENDPOINT("/api/challenge/%s/accept", challenge_id);
    return api_post(_uri);
}

bool ApiClient::decline_challenge(const char *challenge_id, const char *reason)
{
    //LOGD("%s(%s)", __func__, challenge_id);

    SET_ENDPOINT("/api/challenge/%s/decline", challenge_id);
    if (NULL == reason) {
        reason = "generic";
    }
    return api_post(_uri, (const uint8_t *)reason, strlen(reason));
}

bool ApiClient::create_seek(const challenge_st *ps_challenge)
{
    int len = SET_PAYLOAD("rated=%s&time=%u&increment=%u&variant=standard&color=%s",
                        ps_challenge->b_rated ? "true" : "false",
                        ps_challenge->u16_clock_limit / 60, // in minutes
                        ps_challenge->u8_clock_increment,   // in seconds
                        ps_challenge->b_color ? "white" : "black");

    LOGD("play as %s vs %s:\r\n%s", ps_challenge->b_color ? "white" : "black", ps_challenge->ac_user, _rsp_buf);
    return api_post("/api/board/seek", (const uint8_t *)_rsp_buf, len);
}

// create seek
bool ApiClient::create_challenge(const challenge_st *ps_challenge, const char *fen)
{
    int payload_len = 0;

    SET_ENDPOINT("/api/challenge/%s", ps_challenge->ac_user);

    if (ps_challenge->e_player <= PLAYER_AI_LEVEL_HIGH)
    {
        payload_len = SET_PAYLOAD("level=%d&fen=%s",
                        (PLAYER_AI_LEVEL_HIGH == ps_challenge->e_player) ? (8) : ((PLAYER_AI_LEVEL_MEDIUM == ps_challenge->e_player) ? (5) : (2)),
                        fen);
    }
    else if ((PLAYER_BOT_MAIA9 == ps_challenge->e_player) || (PLAYER_CUSTOM == ps_challenge->e_player))
    {
        payload_len = SET_PAYLOAD("rated=%s&keepAliveStream=%s&rules=%s",
                        ps_challenge->b_rated ? "true" : "false", "false", "noRematch,noClaimWin,noEarlyDraw");
    }
    else
    {
        return create_seek(ps_challenge);
    }

    payload_len += snprintf(_rsp_buf + payload_len, sizeof(_rsp_buf) - payload_len,
                            "&clock.limit=%u&clock.increment=%u&color=%s&variant=%s",
                            ps_challenge->u16_clock_limit,
                            ps_challenge->u8_clock_increment,
                            ps_challenge->b_color ? "white" : "black",
                            "standard");

    LOGD("play as %s vs %s:\r\n%s", ps_challenge->b_color ? "white" : "black", ps_challenge->ac_user, _rsp_buf);
    return api_post(_uri, (const uint8_t *)_rsp_buf, payload_len);
}

bool ApiClient::cancel_challenge(const char *challenge_id)
{
    LOGD("%s(%s)", __func__, challenge_id);
    // to do
    delayms(1000);
    return false;
}

} // namespace lichess
