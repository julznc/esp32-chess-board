#ifndef CHESS_H
#define CHESS_H

#include <Arduino.h>


namespace chess
{

typedef enum {
    _NONE   = 0, // unknown
    PAWN    = 'p',
    KNIGHT  = 'n',
    BISHOP  = 'b',
    ROOK    = 'r',
    QUEEN   = 'q',
    KING    = 'k'
} piece_et;

typedef enum {
    BLACK = 0,
    WHITE = 1
} color_et;

/* 8-bit piece */
#define PIECE_TYPE(u8_piece)            (tolower(u8_piece))
#define PIECE_COLOR(u8_piece)           (isupper(u8_piece))
#define MAKE_PIECE(b_color, u7_type)    (b_color ? toupper(u7_type) : tolower(u7_type))
#define VALID_PIECE(u7_type)            ((chess::PAWN==u7_type) || (chess::KNIGHT==u7_type) || (chess::BISHOP==u7_type) || \
                                         (chess::ROOK==u7_type) || (chess::QUEEN==u7_type)  || (chess::KING==u7_type))

const char *color_to_string(uint8_t b_color);
const char *piece_to_string(uint8_t u7_type);


} // namespace chess


#endif // CHESS_H
