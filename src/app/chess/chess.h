
#pragma once

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
    BLACK   = 0,
    WHITE   = 1
} color_et;

/* 8-bit piece */
#define PIECE_TYPE(u8_piece)            (tolower(u8_piece))
#define PIECE_COLOR(u8_piece)           (isupper(u8_piece))
#define PIECE_INT(u7_type)              (PAWN==(u7_type) ? 1 : (KNIGHT==(u7_type) ? 2 : (BISHOP==(u7_type) ? 3 : (ROOK==(u7_type) ? 4 : (QUEEN==(u7_type) ? 5 : (KING==(u7_type) ? 6 : 0))))))
#define SWAP_COLOR(color)               ((color) == WHITE ? BLACK : WHITE)
#define MAKE_PIECE(b_color, u7_type)    ((uint8_t)(b_color ? toupper(u7_type) : tolower(u7_type)))
#define VALID_PIECE(u7_type)            ((chess::PAWN==u7_type) || (chess::KNIGHT==u7_type) || (chess::BISHOP==u7_type) || \
                                         (chess::ROOK==u7_type) || (chess::QUEEN==u7_type)  || (chess::KING==u7_type))

} // namespace chess
