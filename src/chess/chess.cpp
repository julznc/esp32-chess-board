
#include "globals.h"
#include "chess.h"


namespace chess
{


const char *color_to_string(uint8_t b_color)
{
    if (b_color) {
        return "WHITE";
    }
    return "BLACK";
}

const char *piece_to_string(uint8_t u7_type)
{
    switch (u7_type)
    {
    #define STR_PIECE(t) case t: return #t;
    STR_PIECE(PAWN);
    STR_PIECE(KNIGHT);
    STR_PIECE(BISHOP);
    STR_PIECE(ROOK);
    STR_PIECE(QUEEN);
    STR_PIECE(KING);
    #undef STR_PIECE
    }
    return "UNKNOWN";
}

} // namespace chess
