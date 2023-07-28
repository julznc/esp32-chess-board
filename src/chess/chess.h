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
#define MAKE_PIECE(b_color, u7_type)    ((uint8_t)(b_color ? toupper(u7_type) : tolower(u7_type)))
#define VALID_PIECE(u7_type)            ((chess::PAWN==u7_type) || (chess::KNIGHT==u7_type) || (chess::BISHOP==u7_type) || \
                                         (chess::ROOK==u7_type) || (chess::QUEEN==u7_type)  || (chess::KING==u7_type))


typedef enum {
    a8 =   0, b8 =   1, c8 =   2, d8 =   3, e8 =   4, f8 =   5, g8 =   6, h8 =   7,
    a7 =  16, b7 =  17, c7 =  18, d7 =  19, e7 =  20, f7 =  21, g7 =  22, h7 =  23,
    a6 =  32, b6 =  33, c6 =  34, d6 =  35, e6 =  36, f6 =  37, g6 =  38, h6 =  39,
    a5 =  48, b5 =  49, c5 =  50, d5 =  51, e5 =  52, f5 =  53, g5 =  54, h5 =  55,
    a4 =  64, b4 =  65, c4 =  66, d4 =  67, e4 =  68, f4 =  69, g4 =  70, h4 =  71,
    a3 =  80, b3 =  81, c3 =  82, d3 =  83, e3 =  84, f3 =  85, g3 =  86, h3 =  87,
    a2 =  96, b2 =  97, c2 =  98, d2 =  99, e2 = 100, f2 = 101, g2 = 102, h2 = 103,
    a1 = 112, b1 = 113, c1 = 114, d1 = 115, e1 = 116, f1 = 117, g1 = 118, h1 = 119
} square_et;

#define SQUARE_TO_IDX(sq)               (((7 - ((sq)>>4)) << 3) + ((sq) & 0xF))
#define IDX_TO_SQUARE(idx)              (((7 - ((idx)>>3)) << 4) + ((idx) & 0x7))
#define RANK(sq)                        ((sq) >> 4) // row
#define FILE(sq)                        ((sq) & 0xF) // column
#define ALGEBRAIC(sq)                   FILE(sq) + 'a', 8 - RANK(sq)



void init(void);
void loop(void);

const char *color_to_string(uint8_t b_color);
const char *piece_to_string(uint8_t u7_type);


} // namespace chess


#endif // CHESS_H
