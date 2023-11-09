
#pragma once

#include <ctype.h>
#include <stdint.h>

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

#define START_FEN                       "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
#define FEN_BUFF_LEN                    (80)
#define IS_START_FEN(fen)               (0 == strncmp(fen, START_FEN, 43))

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


typedef enum {
    BIT_NORMAL       = (1<<0),
    BIT_CAPTURE      = (1<<1),
    BIT_BIG_PAWN     = (1<<2),
    BIT_EP_CAPTURE   = (1<<3),
    BIT_PROMOTION    = (1<<4),
    BIT_KSIDE_CASTLE = (1<<5),
    BIT_QSIDE_CASTLE = (1<<6)
} movemask_et;

typedef enum {
    RANK_1  = 7,
    RANK_2  = 6,
    RANK_3  = 5,
    RANK_4  = 4,
    RANK_5  = 3,
    RANK_6  = 2,
    RANK_7  = 1,
    RANK_8  = 0
} rank_et;

typedef struct move {
    struct move *next;
    uint8_t from;
    uint8_t to;
    uint8_t piece;
    uint8_t flags;
    uint8_t captured;
    uint8_t promoted;
} move_st;

typedef struct {
    uint8_t  turn;          // which color to move
    uint8_t  ep_square;     // en-pasant square
    uint8_t  castling[2];   // castling rights
    uint16_t half_moves;
    uint16_t move_number;
    uint8_t  kings[2];      // kings position
    uint8_t  valid;         // (bool) false = busy checking
} stats_st;

typedef struct record {
    struct record *next;
    move_st  move;
    stats_st stats;
} record_st;

typedef struct {
    uint8_t    board[128];  // position
    stats_st   stats;
    record_st *history;
} game_st;


void init(void);
void loop(uint32_t ms_last_changed);

const char *generate_fen(const game_st *p_game);

bool attacked(const uint8_t *board, uint8_t color, uint8_t square);
#define KING_ATTACKED(game, color)  attacked((game)->board, SWAP_COLOR((color)),  (game)->stats.kings[(color)])
#define IN_CHECK(game)              KING_ATTACKED((game), (game)->stats.turn)

void init_game(game_st *p_game);
void make_move(game_st *p_game, const move_st *move);
bool undo_move(game_st *p_game, move_st *last=nullptr);
move_st *generate_moves(game_st *p_game);
bool move_to_san(move_st *list /*moves list*/, const move_st *p_move /*convert to SAN*/, char *san_buf, uint8_t buf_sz);
bool find_move(game_st *p_game /*board*/, move_st *list /*moves list*/, const uint8_t *scan /*input or desired raw position*/, move_st *p_move /*found move*/);
uint8_t hint_moves(game_st *p_game /*board*/, move_st *list /*moves list*/, const uint8_t *scan, uint8_t *squares_buf, uint8_t max_count);
void clear_moves(move_st **plist);

const char *color_to_string(uint8_t b_color);
const char *piece_to_string(uint8_t u7_type);

// api's
const stats_st *get_position(char *fen /*current position*/, char *move /*last uci move*/);
bool get_position(char *fen);
bool get_last_move(char *move /*uci*/);
bool get_pgn(const char **pgn);
bool continue_game(const char *expected_fen); // continue game from position
bool queue_move(const char *move);


} // namespace chess
