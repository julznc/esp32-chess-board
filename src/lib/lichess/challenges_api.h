
#pragma once

namespace lichess
{

typedef enum {
    VARIANT_UNKNOWN,
    VARIANT_STANDARD,
    //VARIANT_CHESS960, // to do
    //VARIANT_CRAZYHOUSE,
    //VARIANT_ANTICHESS,
    //VARIANT_ATOMIC,
} game_variant_et;  // limit to standard only

typedef enum {
    SPEED_UNKNOWN,
    //SPEED_ULTRABULLET,
    //SPEED_BULLET,
    SPEED_BLITZ,
    SPEED_RAPID,
    SPEED_CLASSICAL,
    //SPEED_CORRESPONDENCE,
} game_speed_et;

typedef enum {
    CHALLENGE_NONE,
    CHALLENGE_CREATED,  // incomming
    CHALLENGE_ACCEPTED,
    CHALLENGE_CANCELED,
    CHALLENGE_DECLINED
} challenge_type_et;

typedef enum {
    PLAYER_AI_LEVEL_LOW = 0,    // Fairy-Stockfish 14 Level 2
    PLAYER_AI_LEVEL_MEDIUM,     // Fairy-Stockfish 14 Level 5
    PLAYER_AI_LEVEL_HIGH,       // Fairy-Stockfish 14 Level 8 // max
    PLAYER_BOT_MAIA9,           // https://lichess.org/@/maia9
#if 0 // https://lichess.org/player/bots
    PLAYER_BOT_HUMAIA_S,        // https://lichess.org/@/Humaia-Strong
    PLAYER_BOT_OPENINGS,        // https://lichess.org/@/OpeningsBot  -- up to 20+10 only
    PLAYER_BOT_FIREFISH,        // https://lichess.org/@/FireFishBOT_v2
    PLAYER_BOT_AKS_MANTISSA,    // https://lichess.org/@/AKS-Mantissa
    PLAYER_BOT_BORIS_TRAPSKY,   // https://lichess.org/@/Boris-Trapsky
    PLAYER_BOT_FROZENIGHT,      // https://lichess.org/@/FrozenightEngine
#endif
    PLAYER_CUSTOM,              // user-defined/configured opponent (default to maia9 bot)
    PLAYER_RANDOM_SEEK,         // random player (create public seek)

    PLAYER_FIRST_IDX    = PLAYER_AI_LEVEL_LOW,
    PLAYER_LAST_IDX     = PLAYER_RANDOM_SEEK
} challenge_player_et;

typedef struct
{
    char            ac_id[16];          // for url
    char            ac_user[32];        // either challenger or destUser
    game_variant_et e_variant;
    int             e_player;           // challenge_player_et
    game_speed_et   e_speed;
    uint16_t        u16_clock_limit;    // seconds
    uint8_t         u8_clock_increment; // seconds
    bool            b_rated;
    bool            b_color;            // challenger's color (true = white)
} challenge_st;


// return challenge_type_st or negative for error
int parse_challenge_event(const cJSON *event, challenge_st *ps_challenge /*output*/);

} // namespace lichess
