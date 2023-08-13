#ifndef __LICHESS_CHALLENGES_API_H__
#define __LICHESS_CHALLENGES_API_H__

#include <ArduinoJson.h>


namespace lichess
{

typedef enum
{
    VARIANT_UNKNOWN,
    VARIANT_STANDARD,
    //VARIANT_CHESS960, // to do
    //VARIANT_CRAZYHOUSE,
    //VARIANT_ANTICHESS,
    //VARIANT_ATOMIC,
} game_variant_et;  // limit to standard only

typedef enum
{
    SPEED_UNKNOWN,
    //SPEED_ULTRABULLET,
    //SPEED_BULLET,
    SPEED_BLITZ,
    SPEED_RAPID,
    SPEED_CLASSICAL,
    //SPEED_CORRESPONDENCE,
} game_speed_et;

typedef enum
{
    CHALLENGE_NONE,
    CHALLENGE_CREATED,  // incomming
    CHALLENGE_ACCEPTED,
    CHALLENGE_CANCELED,
    CHALLENGE_DECLINED
} challenge_type_et;

typedef struct
{
    char            ac_id[16];          // for url
    char            ac_user[32];        // either challenger or destUser
    game_variant_et e_variant;
    game_speed_et   e_speed;
    uint16_t        u16_clock_limit;    // seconds
    uint8_t         u8_clock_increment; // seconds
    bool            b_rated;
    bool            b_color;            // challenger's color (true = white)
} challenge_st;


// return challenge_type_st or negative for error
int parse_challenge_event(DynamicJsonDocument &json, challenge_st *ps_challenge /*output*/);

bool accept_challenge(const char *challenge_id);
bool decline_challenge(const char *challenge_id, const char *reason=NULL);

bool create_challenge(const challenge_st *ps_challenge, const char *fen);
bool cancel_challenge(const char *challenge_id);

} // namespace lichess

#endif // __LICHESS_CHALLENGES_API_H__
