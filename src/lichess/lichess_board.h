#ifndef __LICHESS_BOARD_API_H__
#define __LICHESS_BOARD_API_H__


namespace lichess
{

typedef enum
{
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

typedef enum
{
    GAME_STREAM_STATE_UNKNOWN,
    GAME_STREAM_STATE_FULL,         // "gameFull"
    GAME_STREAM_STATE_CURRENT,      // "gameState"
    GAME_STREAM_STATE_FINISH,       // "gameFinish" from /api/stream/event
    GAME_STREAM_STATE_CHATLINE,     // "chatLine"
    GAME_STREAM_STATE_OPPONENTGONE, // "opponentGone"
} game_stream_state_et;


typedef struct
{
    char            ac_id[16];      // for url
    game_state_et   e_state;
} game_st;


int parse_game_event(DynamicJsonDocument &json, game_st *ps_game /*output*/);
int parse_game_state(DynamicJsonDocument &json, game_st *ps_game /*output*/);

bool game_move(const char *game_id, const char *move_uci);
bool game_abort(const char *game_id);
bool game_resign(const char *game_id);
bool handle_draw(const char *game_id, bool b_accept);
bool handle_takeback(const char *game_id, bool b_accept);

} // namespace lichess

#endif // __LICHESS_BOARD_API_H__
