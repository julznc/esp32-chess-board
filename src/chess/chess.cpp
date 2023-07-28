
#include "globals.h"
#include "board/board.h"
#include "ui/ui.h"
#include "chess.h"


namespace chess
{

static game_st          st_game;
SemaphoreHandle_t       mtx = NULL;
static move_st          last_move;
static move_st          pending_move;

static const uint8_t   *pu8_pieces = NULL;
static uint8_t          au8_prev_pieces[64];
static char             fen_buf[80];
static bool             pending_led = false;
static bool             skip_start_fen = false;

static struct {
    char san_black[16];
    char san_white[16];
} s_move_stack[256];


static uint8_t AU8_START_PIECES[64] =
{
    MAKE_PIECE(WHITE, ROOK), MAKE_PIECE(WHITE, KNIGHT), MAKE_PIECE(WHITE, BISHOP), MAKE_PIECE(WHITE, QUEEN),
    MAKE_PIECE(WHITE, KING), MAKE_PIECE(WHITE, BISHOP), MAKE_PIECE(WHITE, KNIGHT), MAKE_PIECE(WHITE, ROOK),
    MAKE_PIECE(WHITE, PAWN), MAKE_PIECE(WHITE, PAWN),   MAKE_PIECE(WHITE, PAWN),   MAKE_PIECE(WHITE, PAWN),
    MAKE_PIECE(WHITE, PAWN), MAKE_PIECE(WHITE, PAWN),   MAKE_PIECE(WHITE, PAWN),   MAKE_PIECE(WHITE, PAWN),
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    MAKE_PIECE(BLACK, PAWN), MAKE_PIECE(BLACK, PAWN),   MAKE_PIECE(BLACK, PAWN),   MAKE_PIECE(BLACK, PAWN),
    MAKE_PIECE(BLACK, PAWN), MAKE_PIECE(BLACK, PAWN),   MAKE_PIECE(BLACK, PAWN),   MAKE_PIECE(BLACK, PAWN),
    MAKE_PIECE(BLACK, ROOK), MAKE_PIECE(BLACK, KNIGHT), MAKE_PIECE(BLACK, BISHOP), MAKE_PIECE(BLACK, QUEEN),
    MAKE_PIECE(BLACK, KING), MAKE_PIECE(BLACK, BISHOP), MAKE_PIECE(BLACK, KNIGHT), MAKE_PIECE(BLACK, ROOK),
};

static inline void lock(void)
{
    (void)xSemaphoreTake(mtx, portMAX_DELAY);
}

static inline void unlock(void)
{
    (void)xSemaphoreGive(mtx);
}

static inline void load_position(const uint8_t *raw, bool initial)
{

    if (initial)
    {
        memset(&st_game, 0, sizeof(st_game));
    }

    for (uint8_t rank = 0; rank < 8; rank++)
    {
        for (uint8_t file = 0; file < 8; file++)
        {
            // note: inverted rank order
            uint8_t idx = ((7-rank) << 4) + file;
            uint8_t piece = *raw++;
            st_game.board[idx] = piece;
            if (KING == PIECE_TYPE(piece)) {
                // one king only per color
                // assert(0 == st_game.stats.kings[PIECE_COLOR(piece)]);
                st_game.stats.kings[PIECE_COLOR(piece)] = idx;
            }
            //printf("%3d=%02x ", idx, st_game.board[idx]);
        }
        //printf("\r\n");
    }

    if (initial)
    {
        // default to white's turn
        st_game.stats.turn = WHITE;

    #if 1 // castling rights (not accurate)
        if (MAKE_PIECE(WHITE, KING) == st_game.board[e1])
        {
            if (MAKE_PIECE(WHITE, ROOK) == st_game.board[a1]) {
                st_game.stats.castling[WHITE] |= BIT_QSIDE_CASTLE;
            }
            if (MAKE_PIECE(WHITE, ROOK) == st_game.board[h1]) {
                st_game.stats.castling[WHITE] |= BIT_KSIDE_CASTLE;
            }
        }

        if (MAKE_PIECE(BLACK, KING) == st_game.board[e8])
        {
            if (MAKE_PIECE(BLACK, ROOK) == st_game.board[a8]) {
                st_game.stats.castling[BLACK] |= BIT_QSIDE_CASTLE;
            }
            if (MAKE_PIECE(BLACK, ROOK) == st_game.board[h8]) {
                st_game.stats.castling[BLACK] |= BIT_KSIDE_CASTLE;
            }
        }
        //LOGD("castling %02x %02x", st_game.stats.castling[WHITE], st_game.stats.castling[BLACK]);
    #endif

    #if 0 // to do
        st_game.stats.ep_square = 0;
    #endif

    #if 0 // to do
        st_game.stats.half_moves = 0;
    #endif

    #if 1 // to do
        st_game.stats.move_number = 1;
    #endif

    }

    LOGD("fen: %s", generate_fen(&st_game));
}

void init(void)
{
    if (NULL == mtx)
    {
        mtx = xSemaphoreCreateMutex();
        ASSERT((NULL != mtx), "chess mutex allocation failed");

        pu8_pieces = brd::pu8_pieces();
    }

    memset(&last_move, 0, sizeof(move_st));
    memset(&pending_move, 0, sizeof(move_st));

    memset(&s_move_stack, 0, sizeof(s_move_stack));
    strncpy(s_move_stack[0].san_white, "...", 15);
    strncpy(s_move_stack[0].san_black, "...", 15);

}

static inline void show_turn(void)
{
    static bool state = false;
    if (WHITE == st_game.stats.turn) {
        ui::leds::setColor(d5, state ? ui::leds::LED_GREEN : ui::leds::LED_OFF);
        ui::leds::setColor(e4, state ? ui::leds::LED_OFF : ui::leds::LED_GREEN);
        ui::leds::setColor(d4, ui::leds::LED_OFF);
        ui::leds::setColor(e5, ui::leds::LED_OFF);
    } else {
        ui::leds::setColor(d4, state ? ui::leds::LED_GREEN : ui::leds::LED_OFF);
        ui::leds::setColor(e5, state ? ui::leds::LED_OFF : ui::leds::LED_GREEN);
        ui::leds::setColor(d5, ui::leds::LED_OFF);
        ui::leds::setColor(e4, ui::leds::LED_OFF);
    }
    state = !state; // blink
}

static inline void show_diff(const uint8_t *prev)
{
    for (uint8_t i = 0; i < 64; i++)
    {
        if (pu8_pieces[i] != prev[i]) {
            ui::leds::setColor(i>>3, i&7, ui::leds::LED_RED);
        }
    }
}

static inline void show_checked(void)
{
    static bool state = false;
    if (IN_CHECK(&st_game))
    {
        uint8_t idx = SQUARE_TO_IDX(st_game.stats.kings[st_game.stats.turn]);
        ui::leds::setColor(idx>>3, idx&7, state ? ui::leds::LED_RED : ui::leds::LED_OFF);
        state = !state; // blink
    }
}

static inline void show_pending(void)
{
    pending_led = !pending_led; // blink

    if (pending_move.piece)
    {
        ui::leds::setColor(pending_move.from, pending_led ? ui::leds::LED_OFF : ui::leds::LED_GREEN);
        ui::leds::setColor(pending_move.to, pending_led ? ui::leds::LED_GREEN : ui::leds::LED_OFF);
    }
}

static inline void show_move(void)
{
    if (last_move.piece)
    {
        ui::leds::setColor(last_move.from, ui::leds::LED_YELLOW);
        if (pending_move.piece && (pending_move.to == last_move.from) && pending_led) {
            ui::leds::setColor(last_move.from, ui::leds::LED_GREEN);
        }

        ui::leds::setColor(last_move.to, ui::leds::LED_YELLOW);
        if (pending_move.piece && (pending_move.to == last_move.to) && pending_led) {
            ui::leds::setColor(last_move.to, ui::leds::LED_GREEN);
        }
    }
}

static inline void display_stats(void)
{
    const stats_st *stats = &st_game.stats;

    DISPLAY_TEXT1(0, 32, "%c %u %u",
                WHITE == stats->turn ? 'w' : 'b',
                stats->half_moves, stats->move_number);
    DISPLAY_CLEAR_ROW(44, 20);
    //DISPLAY_TEXT2(0, 44, "%s %s", san_white, san_black);
    DISPLAY_TEXT2(0, 44, "%.15s %.15s",
                s_move_stack[(WHITE == stats->turn) && (stats->move_number > 1) ? (stats->move_number - 2) : (stats->move_number - 1)].san_white,
                s_move_stack[(stats->move_number > 1) ? (stats->move_number - 2) : (stats->move_number - 1)].san_black);
}

static inline void do_move(move_st *list, move_st *move)
{
    char san_buf[16];

    lock();

    make_move(&st_game, move);
    move_to_san(list, move, san_buf, sizeof(san_buf) - 1);

    memcpy(au8_prev_pieces, pu8_pieces, sizeof(au8_prev_pieces));
    memcpy(&last_move, move, sizeof(move_st));
    //memset(&pending_move, 0, sizeof(move_st));

    if (move->flags & BIT_PROMOTION) {
        // set actual promoted piece
        uint8_t promoted = pu8_pieces[SQUARE_TO_IDX(move->to)];
        //LOGD("change %02x to %02x", st_game.board[move->to], promoted);
        char ch = PIECE_TYPE(promoted);
        st_game.board[move->to] = promoted;
        snprintf(&san_buf[strlen(san_buf)], sizeof(san_buf) - 4, "=%c", toupper(ch));
    }

    unlock();

    move_st *next_moves = generate_moves(&st_game);
    if (next_moves) {
        if (IN_CHECK(&st_game)) {
            strcat(san_buf, "+"); // in-check only
        }
        clear_moves(&next_moves);
    } else if (IN_CHECK(&st_game)) {
        strcat(san_buf, "#"); // checkmate!
        LOGI("matyas!!!");
    } else {
        strcat(san_buf, "sm"); // stalemate
    }

    LOGD("%-5s : \"%s\"", san_buf, generate_fen(&st_game));
    if (BLACK == st_game.stats.turn) {
        strncpy(s_move_stack[st_game.stats.move_number - 1].san_white, san_buf, 15);
    } else {
        strncpy(s_move_stack[st_game.stats.move_number - 2].san_black, san_buf, 15);
    }
    //DISPLAY_CLEAR();
    //DISPLAY_TEXT(4, 48, 1, "%s", san_buf);
    display_stats();
}

static inline bool check_start_fen(void)
{
    uint8_t u8_diff = 0;

    if (ui::btn::pb1.getCount())
    {
        skip_start_fen = true;
    }
    else
    {
        for (uint8_t u8_idx = 0; u8_idx < sizeof(AU8_START_PIECES); u8_idx++)
        {
            if (pu8_pieces[u8_idx] != AU8_START_PIECES[u8_idx])
            {
                ui::leds::setColor(u8_idx>>3, u8_idx&7, ui::leds::LED_RED);
                u8_diff++;
            }
        }
    }

    ui::leds::update();
    return (0 == u8_diff);
}

void loop(void)
{
    static uint32_t u32_last_checked = 0;
    static bool     b_start_fen = false;

    uint8_t au8_allowed_squares[28];
    uint8_t u8_squares_count;

    ui::leds::clear();

    if (false == b_start_fen)
    {
        b_start_fen = skip_start_fen || check_start_fen();
        if (b_start_fen)
        {
            memcpy(au8_prev_pieces, pu8_pieces, sizeof(au8_prev_pieces));
            load_position(pu8_pieces, true);
        }
    }
    else if (0 == memcmp(au8_prev_pieces, pu8_pieces, sizeof(au8_prev_pieces)))
    {
        //LOGD("no change yet");
        if ((0 == last_move.piece) && (0 == pending_move.piece)) {
            show_turn();
            display_stats();
        } else if (1) { //!hide_moves) {
            show_pending();
            show_move();
            show_checked();
        }
        ui::leds::update();
    }
    else if (millis() - u32_last_checked < 500)
    {
        //LOGD("blanking");
    }
    else
    {
        move_st move;
        move_st *moves_list = generate_moves(&st_game);

                // exact move
        if (find_move(&st_game, moves_list, pu8_pieces, &move))
        {
            do_move(moves_list, &move);
        }
        // lift a piece ?
        else if (0 != (u8_squares_count = hint_moves(&st_game, moves_list, pu8_pieces, au8_allowed_squares, sizeof(au8_allowed_squares))))
        {
          #if 0
            uint8_t piece = au8_prev_pieces[SQUARE_TO_IDX(au8_allowed_squares[0])];
            LOGD("touch %s %s on %c%u", color_to_string(PIECE_COLOR(piece)), piece_to_string(PIECE_TYPE(piece)),
                ALGEBRAIC(au8_allowed_squares[0]));
          #endif
            ui::leds::setColor(au8_allowed_squares[0], ui::leds::LED_ORANGE);
            for (uint8_t i = 1; i < u8_squares_count; i++) {
                ui::leds::setColor(au8_allowed_squares[i], ui::leds::LED_GREEN);
                //LOGD("%u/%u %c%u", i, u8_squares_count - 1, ALGEBRAIC(au8_allowed_squares[i]));
            }
            ui::leds::update();
        }
        else
        {
            // is continuation ?
            lock();
            if (undo_move(&st_game, &move))
            {
                unlock();
                clear_moves(&moves_list);
                moves_list = generate_moves(&st_game);
                if (find_move(&st_game, moves_list, pu8_pieces, &move))
                {
                    do_move(moves_list, &move);
                }
                else
                {
                    //LOGW("continuation not found");
                    show_diff(au8_prev_pieces);
                    ui::leds::update();
                    lock();
                    make_move(&st_game, &move); // redo last
                    unlock();
                }
            }
            else
            {
                //LOGW("move not found");
                show_diff(au8_prev_pieces);
                ui::leds::update();
            }
            unlock();
        }

        clear_moves(&moves_list);

        u32_last_checked = millis();
    }
}

const char *generate_fen(const game_st *p_game)
{
    char *fen = fen_buf;
    uint8_t empty = 0;
    uint8_t pos = a8;

    memset(fen_buf, 0, sizeof(fen_buf));
    while(pos <= h1)
    {
        uint8_t piece = p_game->board[pos];
        //printf("%02x ", piece);
        if (_NONE == piece) {
            empty++;
        } else {
            if (empty > 0) {
                *fen++ = '0' + empty;
                empty = 0;
            }
            uint8_t color = PIECE_COLOR(piece);
            char ch  = PIECE_TYPE(piece);
            *fen++ = (WHITE == color) ? toupper(ch) : ch;
        }
        pos++;
        if ((pos & 0xF) > 7) {
            //printf("\r\n");
            if (empty > 0) {
                *fen++ = '0' + empty;
                empty = 0;
            }
            if (pos < h1) {
                *fen++ += '/';
            }
            pos += 8;
        }
    }

    *fen++ = ' ';
    *fen++ = p_game->stats.turn == WHITE ? 'w' : 'b';

    *fen++ = ' ';
    if ((0 == p_game->stats.castling[WHITE]) && (0 == p_game->stats.castling[BLACK])) {
        *fen++ = '-';
    }
    else {
        if (p_game->stats.castling[WHITE] & BIT_KSIDE_CASTLE)
            *fen++ = 'K';
        if (p_game->stats.castling[WHITE] & BIT_QSIDE_CASTLE)
            *fen++ = 'Q';
        if (p_game->stats.castling[BLACK] & BIT_KSIDE_CASTLE)
            *fen++ = 'k';
        if (p_game->stats.castling[BLACK] & BIT_QSIDE_CASTLE)
            *fen++ = 'q';
    }

    *fen++ = ' ';
    *fen++ = '-'; // todo: ep

    *fen++ = ' ';
    fen += snprintf(fen, 4, "%u", p_game->stats.half_moves);

    *fen++ = ' ';
    fen += snprintf(fen, 4, "%u", p_game->stats.move_number);

    return fen_buf;
}

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
