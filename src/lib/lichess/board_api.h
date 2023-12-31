
#pragma once

namespace lichess
{

typedef enum {
    GAME_STATE_UNKNOWN          = 0,
    GAME_STATE_CREATED          = 10,
    GAME_STATE_STARTED          = 20,
    GAME_STATE_ABORTED          = 25,
    GAME_STATE_MATE             = 30,
    GAME_STATE_RESIGN           = 31,
    GAME_STATE_STALEMATE        = 32,
    GAME_STATE_TIMEOUT          = 33,
    GAME_STATE_DRAW             = 34,
    GAME_STATE_OUTOFTIME        = 35,
    GAME_STATE_CHEAT            = 36,
    GAME_STATE_NOSTART          = 37,
    GAME_STATE_UNKNOWNFINISH    = 38,
    GAME_STATE_VARIANTEND       = 60,
} game_state_et; // game status

typedef enum {
    GAME_STREAM_STATE_UNKNOWN,
    GAME_STREAM_STATE_FULL,         // "gameFull"
    GAME_STREAM_STATE_CURRENT,      // "gameState"
    GAME_STREAM_STATE_FINISH,       // "gameFinish" from /api/stream/event
    GAME_STREAM_STATE_CHATLINE,     // "chatLine"
    GAME_STREAM_STATE_OPPONENTGONE, // "opponentGone"
} game_stream_state_et;


typedef struct {
    char            ac_id[16];      // for url
    char            ac_opponent[32];
    char            ac_lastmove[8];
    char            ac_fen[80];
    char            ac_state[16]; // string e_state
    char            ac_moves[2048];
    game_state_et   e_state;
    uint32_t        u32_wtime;
    uint32_t        u32_btime;
    uint32_t        u32_winc;
    uint32_t        u32_binc;
    bool            b_color;        // us; true = white
    bool            b_turn;         // isMyTurn
} game_st;


int parse_game_event(const cJSON *event, game_st *ps_game /*output*/);
int parse_game_state(const cJSON *state, game_st *ps_game /*output*/);

} // namespace lichess
