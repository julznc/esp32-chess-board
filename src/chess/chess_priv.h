#ifndef CHESS_PRIV_H
#define CHESS_PRIV_H

#include "chess.h"


namespace chess
{

const uint8_t KINGS[] = { e8, e1 };

const uint8_t ROOKS[][4] = {
    { a8, BIT_QSIDE_CASTLE, h8, BIT_KSIDE_CASTLE },
    { a1, BIT_QSIDE_CASTLE, h1, BIT_KSIDE_CASTLE }
};

const uint8_t SECOND_RANK[] = { RANK_7, RANK_2 };

const uint8_t SHIFTS[] = {
    /*p*/ 0, // black
    /*p*/ 0, // white
    /*n*/ 1,
    /*b*/ 2,
    /*r*/ 3,
    /*q*/ 4,
    /*k*/ 5
};

const int8_t PIECE_OFFSETS[][8] = {
    /*p*/{  16,  32,  17,  15,   0,  0,  0,  0 }, // black pawn
    /*p*/{ -16, -32, -17, -15,   0,  0,  0,  0 }, // white pawn
    /*n*/{ -18, -33, -31, -14,  18, 33, 31, 14 },
    /*b*/{ -17, -15,  17,  15,   0,  0,  0,  0 },
    /*r*/{ -16,   1,  16,  -1,   0,  0,  0,  0 },
    /*q*/{ -17, -16, -15,   1,  17, 16, 15, -1 },
    /*k*/{ -17, -16, -15,   1,  17, 16, 15, -1 }
};

const int8_t ATTACKS[] = {
    20, 0, 0, 0, 0, 0, 0, 24,  0, 0, 0, 0, 0, 0,20, 0,
     0,20, 0, 0, 0, 0, 0, 24,  0, 0, 0, 0, 0,20, 0, 0,
     0, 0,20, 0, 0, 0, 0, 24,  0, 0, 0, 0,20, 0, 0, 0,
     0, 0, 0,20, 0, 0, 0, 24,  0, 0, 0,20, 0, 0, 0, 0,
     0, 0, 0, 0,20, 0, 0, 24,  0, 0,20, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0,20, 2, 24,  2,20, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 2,53, 56, 53, 2, 0, 0, 0, 0, 0, 0,
     24,24,24,24,24,24,56, 0, 56,24,24,24,24,24,24, 0,
     0, 0, 0, 0, 0, 2,53, 56, 53, 2, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0,20, 2, 24,  2,20, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0,20, 0, 0, 24,  0, 0,20, 0, 0, 0, 0, 0,
     0, 0, 0,20, 0, 0, 0, 24,  0, 0, 0,20, 0, 0, 0, 0,
     0, 0,20, 0, 0, 0, 0, 24,  0, 0, 0, 0,20, 0, 0, 0,
     0,20, 0, 0, 0, 0, 0, 24,  0, 0, 0, 0, 0,20, 0, 0,
    20, 0, 0, 0, 0, 0, 0, 24,  0, 0, 0, 0, 0, 0,20
};

const int8_t RAYS[] = {
    17,  0,  0,  0,  0,  0,  0, 16,  0,  0,  0,  0,  0,  0, 15, 0,
     0, 17,  0,  0,  0,  0,  0, 16,  0,  0,  0,  0,  0, 15,  0, 0,
     0,  0, 17,  0,  0,  0,  0, 16,  0,  0,  0,  0, 15,  0,  0, 0,
     0,  0,  0, 17,  0,  0,  0, 16,  0,  0,  0, 15,  0,  0,  0, 0,
     0,  0,  0,  0, 17,  0,  0, 16,  0,  0, 15,  0,  0,  0,  0, 0,
     0,  0,  0,  0,  0, 17,  0, 16,  0, 15,  0,  0,  0,  0,  0, 0,
     0,  0,  0,  0,  0,  0, 17, 16, 15,  0,  0,  0,  0,  0,  0, 0,
     1,  1,  1,  1,  1,  1,  1,  0, -1, -1,  -1,-1, -1, -1, -1, 0,
     0,  0,  0,  0,  0,  0,-15,-16,-17,  0,  0,  0,  0,  0,  0, 0,
     0,  0,  0,  0,  0,-15,  0,-16,  0,-17,  0,  0,  0,  0,  0, 0,
     0,  0,  0,  0,-15,  0,  0,-16,  0,  0,-17,  0,  0,  0,  0, 0,
     0,  0,  0,-15,  0,  0,  0,-16,  0,  0,  0,-17,  0,  0,  0, 0,
     0,  0,-15,  0,  0,  0,  0,-16,  0,  0,  0,  0,-17,  0,  0, 0,
     0,-15,  0,  0,  0,  0,  0,-16,  0,  0,  0,  0,  0,-17,  0, 0,
   -15,  0,  0,  0,  0,  0,  0,-16,  0,  0,  0,  0,  0,  0,-17
};

} // namespace chess

#endif // CHESS_PRIV_H
