
#include "globals.h"
#include "board/board.h"
#include "ui/ui.h"
#include "chess.h"


namespace chess
{

static game_st          s_game;
SemaphoreHandle_t       mtx = NULL;
static move_st          last_move;
static move_st          pending_move;

static const uint8_t   *pu8_pieces = NULL;
static uint8_t          au8_prev_pieces[64];
static char             ac_fen_buf[80];
static bool             b_pending_led = false;
static bool             b_skip_start_fen = false;
static bool             b_valid_posision = false;

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

static inline uint8_t count_pieces(const uint8_t u8_piece, uint8_t *pau8_sqs=NULL, uint8_t u8_max=0)
{
    uint8_t u8_count = 0;

    for (uint8_t rank = 0; rank < 8; rank++)
    {
        for (uint8_t file = 0; file < 8; file++)
        {
            uint8_t sq = ((7-rank) << 4) + file;
            if (u8_piece == s_game.board[sq])
            {
                if ((u8_count < u8_max) && (NULL != pau8_sqs)) {
                    pau8_sqs[u8_count] = sq;
                }
                u8_count++;
            }
        }
    }

    return u8_count;
}

static inline bool validate_king(void)
{
    if (1 != count_pieces(MAKE_PIECE(WHITE, KING)))
    {
        LOGW("invalid white king");
    }
    else if (1 != count_pieces(MAKE_PIECE(BLACK, KING)))
    {
        LOGW("invalid black king");
    }
    else
    {
        uint8_t sqw = s_game.stats.kings[WHITE];
        uint8_t sqb = s_game.stats.kings[BLACK];
        uint8_t rw = RANK(sqw), fw = FILE(sqw);
        uint8_t rb = RANK(sqb), fb = FILE(sqb);
        uint8_t rdiff = rw > rb ? (rw - rb) : (rb - rw);
        uint8_t fdiff = fw > fb ? (fw - fb) : (fb - fw);
        if ((rdiff > 1) || (fdiff > 1)) {
            // to do: validate king attackers
            return true;
        }
        LOGW("close kayo? white K%c%u black K%c%u", ALGEBRAIC(sqw), ALGEBRAIC(sqb));
    }

    return false;
}

static inline bool validate_pawns(uint8_t color)
{
    uint8_t au8_sqs[8];
    uint8_t u8_cnt = 0;
    bool    b_valid = false;

    if ((u8_cnt = count_pieces(MAKE_PIECE(color, PAWN), au8_sqs, sizeof(au8_sqs))) > 8)
    {
        LOGW("too many %s pawns", color_to_string(color));
    }
    else
    {
        b_valid = true;
        for (uint8_t i = 0; i < u8_cnt; i++) {
            uint8_t rank = RANK(au8_sqs[i]);
            if ((RANK_1 == rank) || ((RANK_8 == rank))) {
                LOGW("invalid %s pawn at %c%u", color_to_string(color), ALGEBRAIC(au8_sqs[i]));
                b_valid = false;
                break;
            }
        }
    }

    return b_valid;
}

static inline bool validate_position(void)
{
    bool    b_valid = false;

    if (!validate_king())
    {
        //
    }
    else if (!validate_pawns(WHITE) || !validate_pawns(BLACK))
    {
        //
    }
    else
    {
        LOGD("white K%c%u black K%c%u", ALGEBRAIC(s_game.stats.kings[WHITE]), ALGEBRAIC(s_game.stats.kings[BLACK]));
        b_valid = true;
    }

    return b_valid;
}

static inline bool load_position(const uint8_t *raw, bool initial)
{
    if (initial)
    {
        memset(&s_game, 0, sizeof(s_game));
    }

    for (uint8_t rank = 0; rank < 8; rank++)
    {
        for (uint8_t file = 0; file < 8; file++)
        {
            // note: inverted rank order
            uint8_t sq = ((7-rank) << 4) + file;
            uint8_t piece = *raw++;
            s_game.board[sq] = piece;
            if (KING == PIECE_TYPE(piece)) {
                // one king only per color
                // assert(0 == s_game.stats.kings[PIECE_COLOR(piece)]);
                s_game.stats.kings[PIECE_COLOR(piece)] = sq;
            }
            //printf("%3d=%02x ", sq, s_game.board[sq]);
        }
        //printf("\r\n");
    }

    if (initial)
    {
        // default to white's turn
        s_game.stats.turn = WHITE;

    #if 1 // castling rights (not accurate)
        if (MAKE_PIECE(WHITE, KING) == s_game.board[e1])
        {
            if (MAKE_PIECE(WHITE, ROOK) == s_game.board[a1]) {
                s_game.stats.castling[WHITE] |= BIT_QSIDE_CASTLE;
            }
            if (MAKE_PIECE(WHITE, ROOK) == s_game.board[h1]) {
                s_game.stats.castling[WHITE] |= BIT_KSIDE_CASTLE;
            }
        }

        if (MAKE_PIECE(BLACK, KING) == s_game.board[e8])
        {
            if (MAKE_PIECE(BLACK, ROOK) == s_game.board[a8]) {
                s_game.stats.castling[BLACK] |= BIT_QSIDE_CASTLE;
            }
            if (MAKE_PIECE(BLACK, ROOK) == s_game.board[h8]) {
                s_game.stats.castling[BLACK] |= BIT_KSIDE_CASTLE;
            }
        }
        //LOGD("castling %02x %02x", s_game.stats.castling[WHITE], s_game.stats.castling[BLACK]);
    #endif

    #if 0 // to do
        s_game.stats.ep_square = 0;
    #endif

    #if 0 // to do
        s_game.stats.half_moves = 0;
    #endif

    #if 1 // to do
        s_game.stats.move_number = 1;
    #endif

    }

    LOGD("fen: %s", generate_fen(&s_game));

    return validate_position();
}

void init(void)
{
    if (NULL == mtx)
    {
        mtx = xSemaphoreCreateMutex();
        ASSERT((NULL != mtx), "chess mutex allocation failed");

        pu8_pieces = brd::pu8_pieces();
    }

    init_game(&s_game);

    memset(&last_move, 0, sizeof(move_st));
    memset(&pending_move, 0, sizeof(move_st));

    memset(&s_move_stack, 0, sizeof(s_move_stack));
    strncpy(s_move_stack[0].san_white, "...", 15);
    strncpy(s_move_stack[0].san_black, "...", 15);

    b_pending_led    = false;
    b_skip_start_fen = false;
    b_valid_posision = false;

}

static inline void show_turn(void)
{
    static bool state = false;

    // blink king's square
    ui::leds::setColor(s_game.stats.kings[s_game.stats.turn], state ? ui::leds::LED_ORANGE : ui::leds::LED_OFF);
    ui::leds::setColor(s_game.stats.kings[SWAP_COLOR(s_game.stats.turn)], ui::leds::LED_OFF);
    if (WHITE == s_game.stats.turn) { // blink e4 square
        ui::leds::setColor(e4, state ? ui::leds::LED_OFF : ui::leds::LED_GREEN);
        ui::leds::setColor(e5, ui::leds::LED_OFF);
    } else { // blink e5 square
        ui::leds::setColor(e5, state ? ui::leds::LED_OFF : ui::leds::LED_GREEN);
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
    if (IN_CHECK(&s_game))
    {
        uint8_t idx = SQUARE_TO_IDX(s_game.stats.kings[s_game.stats.turn]);
        ui::leds::setColor(idx>>3, idx&7, state ? ui::leds::LED_RED : ui::leds::LED_OFF);
        state = !state; // blink
    }
}

static inline void show_pending(void)
{
    b_pending_led = !b_pending_led; // blink

    if (pending_move.piece)
    {
        ui::leds::setColor(pending_move.from, b_pending_led ? ui::leds::LED_OFF : ui::leds::LED_GREEN);
        ui::leds::setColor(pending_move.to, b_pending_led ? ui::leds::LED_GREEN : ui::leds::LED_OFF);
    }
}

static inline void show_move(void)
{
    if (last_move.piece)
    {
        ui::leds::setColor(last_move.from, ui::leds::LED_YELLOW);
        if (pending_move.piece && (pending_move.to == last_move.from) && b_pending_led) {
            ui::leds::setColor(last_move.from, ui::leds::LED_GREEN);
        }

        ui::leds::setColor(last_move.to, ui::leds::LED_YELLOW);
        if (pending_move.piece && (pending_move.to == last_move.to) && b_pending_led) {
            ui::leds::setColor(last_move.to, ui::leds::LED_GREEN);
        }
    }
}

static inline void display_stats(const char *last_san)
{
    const stats_st *stats = &s_game.stats;
    char tmp_fen[24];
    char *fen = tmp_fen;

    *fen++ = stats->turn == WHITE ? 'w' : 'b';
    *fen++ = ' ';
    if ((0 == stats->castling[WHITE]) && (0 == stats->castling[BLACK])) {
        *fen++ = '-';
    }
    else {
        if (stats->castling[WHITE] & BIT_KSIDE_CASTLE)
            *fen++ = 'K';
        if (stats->castling[WHITE] & BIT_QSIDE_CASTLE)
            *fen++ = 'Q';
        if (stats->castling[BLACK] & BIT_KSIDE_CASTLE)
            *fen++ = 'k';
        if (stats->castling[BLACK] & BIT_QSIDE_CASTLE)
            *fen++ = 'q';
    }

    *fen++ = ' ';
    if (!stats->ep_square) {
        *fen++ = '-';
    } else {
        *fen++ = 'a' + FILE(stats->ep_square);
        *fen++ = '0' + 8 - RANK(stats->ep_square);
    }

    *fen++ = ' ';
    fen += snprintf(fen, 4, "%u", stats->half_moves);

    *fen++ = ' ';
    fen += snprintf(fen, 4, "%u", stats->move_number);

    DISPLAY_CLEAR_ROW(20, 8);
    DISPLAY_TEXT1(0, 20, "%.*s. %.*s", 14, tmp_fen, 6, last_san);

#if 0 // to do
    DISPLAY_CLEAR_ROW(44, 20);
    //DISPLAY_TEXT2(0, 44, "%s %s", san_white, san_black);
    DISPLAY_TEXT2(0, 44, "%.15s %.15s",
                s_move_stack[(WHITE == stats->turn) && (stats->move_number > 1) ? (stats->move_number - 2) : (stats->move_number - 1)].san_white,
                s_move_stack[(stats->move_number > 1) ? (stats->move_number - 2) : (stats->move_number - 1)].san_black);
#endif
}

static inline void do_move(move_st *list, move_st *move)
{
    char san_buf[16];

    lock();

    make_move(&s_game, move);
    move_to_san(list, move, san_buf, sizeof(san_buf) - 1);

    memcpy(au8_prev_pieces, pu8_pieces, sizeof(au8_prev_pieces));
    memcpy(&last_move, move, sizeof(move_st));
    memset(&pending_move, 0, sizeof(move_st));

    if (move->flags & BIT_PROMOTION) {
        // set actual promoted piece
        uint8_t promoted = pu8_pieces[SQUARE_TO_IDX(move->to)];
        //LOGD("change %02x to %02x", s_game.board[move->to], promoted);
        char ch = PIECE_TYPE(promoted);
        s_game.board[move->to] = promoted;
        snprintf(&san_buf[strlen(san_buf)], sizeof(san_buf) - 4, "=%c", toupper(ch));
    }

    unlock();

    move_st *next_moves = generate_moves(&s_game);
    if (next_moves) {
        if (IN_CHECK(&s_game)) {
            strcat(san_buf, "+"); // in-check only
        }
        clear_moves(&next_moves);
    } else if (IN_CHECK(&s_game)) {
        strcat(san_buf, "#"); // checkmate!
        LOGI("matyas!!!");
    } else {
        strcat(san_buf, "sm"); // stalemate
    }

    LOGD("%-5s : %s", san_buf, generate_fen(&s_game));
    if (BLACK == s_game.stats.turn) {
        strncpy(s_move_stack[s_game.stats.move_number - 1].san_white, san_buf, 15);
    } else {
        strncpy(s_move_stack[s_game.stats.move_number - 2].san_black, san_buf, 15);
    }
    //DISPLAY_CLEAR();
    //DISPLAY_TEXT(4, 48, 1, "%s", san_buf);
    display_stats(san_buf);
}

static inline bool check_start_fen(void)
{
    uint8_t u8_diff = 0;

    for (uint8_t u8_idx = 0; u8_idx < sizeof(AU8_START_PIECES); u8_idx++)
    {
        if (pu8_pieces[u8_idx] != AU8_START_PIECES[u8_idx])
        {
            ui::leds::setColor(u8_idx>>3, u8_idx&7, ui::leds::LED_RED);
            u8_diff++;
        }
    }

    ui::leds::update();
    return (0 == u8_diff);
}

void loop(void)
{
    static uint32_t u32_last_checked = 0;

    uint8_t au8_allowed_squares[28];
    uint8_t u8_squares_count;

    ui::leds::clear();

    if (!s_game.history) // if no moves yet
    {
        // if upper-left button was pressed ...
        if ((millis() - u32_last_checked > 1000UL) && (1 == ui::btn::pb1.getCount()))
        {
            b_skip_start_fen = true; // allow custom position
            if (b_valid_posision) {
                s_game.stats.turn = SWAP_COLOR(s_game.stats.turn); // toggle turn
                LOGD("new fen: %s", generate_fen(&s_game));
            }
            u32_last_checked = millis();
        }
    }

    if (false == b_valid_posision)
    {
        if (b_skip_start_fen || check_start_fen())
        {
            memcpy(au8_prev_pieces, pu8_pieces, sizeof(au8_prev_pieces));
            b_valid_posision = load_position(pu8_pieces, true);
            b_skip_start_fen = b_valid_posision;
        }
    }
    else if (0 == memcmp(au8_prev_pieces, pu8_pieces, sizeof(au8_prev_pieces)))
    {
        //LOGD("no change yet");
        if ((0 == last_move.piece) && (0 == pending_move.piece)) {
            show_turn();
            display_stats("...");
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
        move_st *moves_list = generate_moves(&s_game);

                // exact move
        if (find_move(&s_game, moves_list, pu8_pieces, &move))
        {
            do_move(moves_list, &move);
        }
        // lift a piece ?
        else if (0 != (u8_squares_count = hint_moves(&s_game, moves_list, pu8_pieces, au8_allowed_squares, sizeof(au8_allowed_squares))))
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
            if (undo_move(&s_game, &move))
            {
                unlock();
                clear_moves(&moves_list);
                moves_list = generate_moves(&s_game);
                if (find_move(&s_game, moves_list, pu8_pieces, &move))
                {
                    do_move(moves_list, &move);
                }
                else
                {
                    //LOGW("continuation not found");
                    show_diff(au8_prev_pieces);
                    ui::leds::update();
                    lock();
                    make_move(&s_game, &move); // redo last
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
    char *fen = ac_fen_buf;
    uint8_t empty = 0;
    uint8_t pos = a8;

    memset(ac_fen_buf, 0, sizeof(ac_fen_buf));
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
    if (!p_game->stats.ep_square) {
        *fen++ = '-';
    } else {
        *fen++ = 'a' + FILE(p_game->stats.ep_square);
        *fen++ = '0' + 8 - RANK(p_game->stats.ep_square);
    }

    *fen++ = ' ';
    fen += snprintf(fen, 4, "%u", p_game->stats.half_moves);

    *fen++ = ' ';
    fen += snprintf(fen, 4, "%u", p_game->stats.move_number);

    return ac_fen_buf;
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

const stats_st *get_position(String &fen /*current position*/, String &move /*last move*/)
{
    lock();
    fen = generate_fen(&s_game);
    unlock();

    (void)get_last_move(move);
    return &s_game.stats;
}

bool get_position(String &fen)
{
    bool b_status = false;
    lock();
    b_status = b_valid_posision;
    if (b_status)
    {
        fen = generate_fen(&s_game);
    }
    unlock();
    return b_status;
}

bool get_last_move(String &move)
{
    bool b_status = false;

    lock();
    if (!s_game.history)
    {
        move = "";
    }
    else
    {
        move  = (char)('a' + FILE(last_move.from));
        move += (char)('0' + 8 - RANK(last_move.from));
        move += (char)('a' + FILE(last_move.to));
        move += (char)('0' + 8 - RANK(last_move.to));
        if (last_move.flags & BIT_PROMOTION) {
            move += (char)PIECE_TYPE(s_game.board[last_move.to]);
        }
        b_status = true;
    }
    unlock();

    return b_status;
}

bool get_pgn(String &pgn)
{
    uint16_t total_moves = s_game.stats.move_number;

    lock();

    if (WHITE == s_game.stats.turn) {
        total_moves--;
    }

    for (uint16_t move_num = 1; move_num <= total_moves; move_num++)
    {
        pgn += String(move_num);
        pgn += ". ";
        pgn += (const char *)s_move_stack[move_num -1].san_white;
        pgn += " ";
        pgn += (const char *)s_move_stack[move_num -1].san_black;
        pgn += " ";
    }

    unlock();
    return true;
}

bool queue_move(const String &move)
{
    //LOGD("%s(%s)", __func__, move.c_str());
    if (move.length() > 3)
    {
        char from_file = move.charAt(0);
        char from_rank = move.charAt(1);
        char to_file   = move.charAt(2);
        char to_rank   = move.charAt(3);
        int8_t from_sq = ((7 - (from_rank - '1')) << 4) + (from_file - 'a');
        int8_t to_sq   = ((7 - (to_rank - '1')) << 4) + (to_file - 'a');
        if ((from_sq < a8) || (from_sq > h1) || (to_sq < a8) || (to_sq > h1))
        {
            LOGW("invalid coord %d %d", from_sq, to_sq);
        }
        else
        {
            if ((to_sq != pending_move.to) || (from_sq != pending_move.from))
            {
                LOGD("queue %c%u %c%u", ALGEBRAIC(from_sq), ALGEBRAIC(to_sq));
                lock();
                pending_move.piece = s_game.board[from_sq];
                pending_move.from = from_sq;
                pending_move.to   = to_sq;
                unlock();
            }
            return true;
        }
    }
    return false;
}

} // namespace chess
