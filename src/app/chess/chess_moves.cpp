
#include <utlist/utlist.h>

#include "globals.h"
#include "chess.h"
#include "chess_priv.h"


namespace chess
{


bool attacked(const uint8_t *board, uint8_t color, uint8_t square)
{
    for (uint8_t i = a8; i <= h1; i++)
    {
        if (FILE(i) > 7)
            continue;

        uint8_t piece = board[i];
        /* if empty square or wrong color */
        if ((0 == piece) || (PIECE_COLOR(piece) != color))
            continue;

        int16_t difference = i - square;
        int16_t index = difference + h1;
        uint8_t type  = PIECE_TYPE(piece);

        if (index < 0 || index >= sizeof(ATTACKS))
            continue;

        if (ATTACKS[index] & (1 << SHIFTS[PIECE_INT(type)])) {
            if (PAWN == PIECE_TYPE(piece)) {
                if (difference > 0) {
                    if (PIECE_COLOR(piece) == WHITE) return true;
                } else {
                    if (PIECE_COLOR(piece) == BLACK) return true;
                }
            } else if ((KNIGHT == type) || (KING == type)) {
                /* if the piece is a knight or a king */
                return true;
            } else {
                int32_t offset = RAYS[index];
                int32_t j = i + offset;

                bool blocked = false;
                while (j != square) {
                    if (0 != board[j]) {
                        blocked = true;
                        break;
                    }
                    j += offset;
                }

                if (!blocked)
                    return true;
            }
        }
    }

    return false;
}

static int compare_moves(move_st *a, move_st *b)
{
    if (!a || !b) return -1;
    if (a->from  != b->from)  return -1;
    if (a->to    != b->to)    return -1;
    return 0; // same "from" and "to"
}

static inline bool add_move(game_st *p_game, move_st **plist, uint8_t from, uint8_t to, uint8_t flags)
{
    move_st move;
    move.from  = from;
    move.to    = to;
    move.piece = p_game->board[from];
    move.flags = flags;
    move.captured = p_game->board[to];
    if (flags & BIT_KSIDE_CASTLE) {
        move.captured = 0; // ignore own rook
    } else if (flags & BIT_EP_CAPTURE) {
        move.captured = MAKE_PIECE(SWAP_COLOR(p_game->stats.turn), PAWN);
    }

    if ((PAWN == PIECE_TYPE(move.piece)) && ((0 == RANK(move.to)) || (7 == RANK(move.to)))) {
        move.flags |= BIT_PROMOTION;
    }

    move_st *elt = NULL;
    LL_SEARCH(*plist, elt, &move, compare_moves);

    if (NULL != elt) {
        //LOGD("already existing %02x from %d to %d", elt->piece, elt->from, elt->to);
        return true;
    } else if (NULL != (elt = (move_st *)malloc(sizeof(move_st)))) {
        memcpy(elt, &move, sizeof(move_st));
        LL_APPEND(*plist, elt);
        return true;
    }

    LOGW("%s() failed", __func__);
    return false;
}

static inline void push(game_st *p_game, const move_st *move)
{
    record_st *record = (record_st *)malloc(sizeof(record_st));
    if (NULL != record) {
        memset(record, 0, sizeof(record_st));
        memcpy(&record->move, move, sizeof(move_st));
        memcpy(&record->stats, &p_game->stats, sizeof(stats_st));
        LL_APPEND(p_game->history, record);
    }
}

void init_game(game_st *p_game)
{
    record_st *history = p_game->history;
    if (history)
    {
        record_st *elt, *tmp;
        LL_FOREACH_SAFE(history, elt, tmp) {
            //LOGD("del %c from %d to %d", elt->move.piece, elt->move.from, elt->move.to);
            LL_DELETE(history, elt);
            free(elt);
        }
    }
    memset(p_game, 0, sizeof(game_st));
}

void make_move(game_st *p_game, const move_st *move)
{
    uint8_t us = PIECE_COLOR(move->piece);
    uint8_t them = SWAP_COLOR(us);
    uint8_t *board = p_game->board;

    push(p_game, move);

    board[move->to]   = board[move->from];
    board[move->from] = 0;

    if (KING == PIECE_TYPE(move->piece))
    {
        p_game->stats.kings[us] = move->to;

        /* if we castled, move the rook next to the king */
        if (move->flags & BIT_KSIDE_CASTLE) {
            board[move->to - 1] = board[move->to + 1];
            board[move->to + 1] = 0;
        } else if (move->flags & BIT_QSIDE_CASTLE) {
            board[move->to + 1] = board[move->to - 2];
            board[move->to - 2] = 0;
        }

        /* turn off castling */
        p_game->stats.castling[us] = 0;
    }
    else
    {
        /* if ep capture, remove the captured pawn */
        if (move->flags & BIT_EP_CAPTURE) {
            board[move->to - PIECE_OFFSETS[us][0]] = 0;
        }

        /* if pawn promotion, replace with new piece */
        if (move->flags & BIT_PROMOTION) {
            board[move->to] = MAKE_PIECE(us, move->promoted ? move->promoted : (uint8_t)QUEEN);
        }
    }

    /* turn off castling if we move a rook */
    if (ROOK == PIECE_TYPE(move->piece)) {
        for (uint8_t i = 0; i < 4; i = i + 2) {
            if (ROOKS[us][i] == move->from) {
                p_game->stats.castling[us] &= ~ROOKS[us][i+1];
            }
        }
    }

    /* turn off castling if we capture a rook */
    if (ROOK == PIECE_TYPE(move->captured)) {
        for (uint8_t i = 0; i < 4; i = i + 2) {
            if (ROOKS[them][i] == move->to) {
                p_game->stats.castling[them] &= ~ROOKS[them][i+1];
            }
        }
    }

    /* if big pawn move, update the en passant square */
    if (move->flags & BIT_BIG_PAWN) {
        p_game->stats.ep_square = move->to - PIECE_OFFSETS[us][0];
    } else {
        p_game->stats.ep_square = 0;
    }

    /* reset the 50 move counter if a pawn is moved or a piece is captured */
    if (PAWN == PIECE_TYPE(move->piece)) {
        p_game->stats.half_moves = 0;
    } else if (move->flags & (BIT_CAPTURE | BIT_EP_CAPTURE)) {
        p_game->stats.half_moves = 0;
    } else {
        p_game->stats.half_moves++;
    }

    if (BLACK == us) {
        p_game->stats.move_number++;
    }

    p_game->stats.turn = them;
}

bool undo_move(game_st *p_game, move_st *last)
{
    if ((NULL == p_game) || (NULL == p_game->history))
        return false;

    uint8_t *board = p_game->board;
    record_st *record = p_game->history;
    while (NULL != record->next) {
        record = record->next; // get last added
    }

    const move_st *move = &record->move;
    memcpy(&p_game->stats, &record->stats, sizeof(stats_st));
    if (last)
        memcpy(last, move, sizeof(move_st));

    uint8_t us = PIECE_COLOR(move->piece);
    uint8_t them = SWAP_COLOR(us);

    board[move->from] = board[move->to];
    board[move->to] = 0;

    if (move->flags & BIT_KSIDE_CASTLE) {
        board[move->to + 1] = board[move->to - 1];
        board[move->to - 1] = 0;
    } else if (move->flags & BIT_QSIDE_CASTLE) {
        board[move->to - 2] = board[move->to + 1];
        board[move->to + 1] = 0;
    } else { // non-castling
        if (move->flags & BIT_PROMOTION) {
            board[move->from] = MAKE_PIECE(PIECE_COLOR(move->piece), PAWN);
        }

        if (move->flags & BIT_CAPTURE) {
            board[move->to] = move->captured;
        } else if (move->flags & BIT_EP_CAPTURE) {
            uint8_t index = move->to - PIECE_OFFSETS[us][0];
            board[index] = MAKE_PIECE(them, PAWN);
        }
    }

    LL_DELETE(p_game->history, record);
    free(record);

    return true;
}

move_st *generate_moves(game_st *p_game)
{
    uint8_t us = p_game->stats.turn;
    uint8_t them = SWAP_COLOR(us);
    uint8_t *board = p_game->board;

    move_st *move_list = NULL;

    for (uint8_t i = 0; i < sizeof(p_game->board); i++)
    {
        uint8_t piece = board[i];
        if ((0 == piece) || (us != PIECE_COLOR(piece)))
            continue;
        //LOGD("piece %02x @ %d", piece, i);

        //uint8_t row = RANK(i);
        //uint8_t col = FILE(i);
        uint8_t type= PIECE_TYPE(piece);

        if (PAWN == type)
        {
            /* single square, non-capturing */
            int16_t square = i + PIECE_OFFSETS[us][0];
            if (0 == board[square]) {
                add_move(p_game, &move_list, i, square, BIT_NORMAL);
                /* double square */
                square = i + PIECE_OFFSETS[us][1];
                if ((0 == board[square]) && (SECOND_RANK[us] == RANK(i))) {
                    add_move(p_game, &move_list, i, square, BIT_BIG_PAWN);
                }
            }

            /* pawn captures */
            for (uint8_t j = 2; j < 4; j++) {
                square = i + PIECE_OFFSETS[us][j];
                if (square & 0x88)
                    continue;
                if ((0 != board[square]) && PIECE_COLOR(board[square]) != us) {
                    add_move(p_game, &move_list, i, square, BIT_CAPTURE);
                } else if ((0 != p_game->stats.ep_square) && (square == p_game->stats.ep_square)) {
                    add_move(p_game, &move_list, i, square, BIT_EP_CAPTURE);
                }
            }
        }
        else // non-pawn
        {
            for (uint8_t j = 0; j < 8; j++) {
                int16_t offset = PIECE_OFFSETS[PIECE_INT(type)][j];
                int16_t square = i;

                while (0 != offset) {
                    square += offset;
                    if (square & 0x88)
                        break;
                    if (0 == board[square]) {
                        add_move(p_game, &move_list, i, square, BIT_NORMAL);
                    } else {
                        if (PIECE_COLOR(p_game->board[square]) == us)
                            break;
                        add_move(p_game, &move_list, i, square, BIT_CAPTURE);
                        break;
                    }

                    /* break, if knight or king */
                    if ((KNIGHT == type) || (KING == type))
                        break;
                } // while loop
            } // offsets

            // castling
            if ((KINGS[us] == i) && (KING == type)) {
                uint8_t castling_from = i;
                // king side
                if (p_game->stats.castling[us] & BIT_KSIDE_CASTLE) {
                    uint8_t castling_to = castling_from + 2;
                    if ((0 == board[castling_from + 1]) &&
                        (0 == board[castling_to]) &&
                        !attacked(board, them, castling_from) &&
                        !attacked(board, them, castling_from + 1) &&
                        !attacked(board, them, castling_to)) {
                            add_move(p_game, &move_list, castling_from, castling_to, BIT_KSIDE_CASTLE);
                    }
                }
                // queen-side
                if (p_game->stats.castling[us] & BIT_QSIDE_CASTLE) {
                    uint8_t castling_to = castling_from - 2;
                    if ((0 == board[castling_from - 1]) &&
                        (0 == board[castling_from - 2]) &&
                        (0 == board[castling_from - 3]) &&
                        !attacked(board, them, castling_from) &&
                        !attacked(board, them, castling_from - 1) &&
                        !attacked(board, them, castling_to)) {
                            add_move(p_game, &move_list, castling_from, castling_to, BIT_QSIDE_CASTLE);
                    }
                }
            } // castling
        }
    } // all squares

    move_st *elt, *tmp;
    // filter out illegal moves
    LL_FOREACH_SAFE(move_list, elt, tmp) {
        make_move(p_game, elt);
        bool valid = !KING_ATTACKED(p_game, us);
        undo_move(p_game);

        if (!valid) {
            //LOGD("invalid move %c %c%u-%c%u", elt->piece, ALGEBRAIC(elt->from), ALGEBRAIC(elt->to));
            LL_DELETE(move_list, elt);
            free(elt);
        //} else {
        //    LOGD("valid move %c %c%u-%c%u", elt->piece, ALGEBRAIC(elt->from), ALGEBRAIC(elt->to));
        }
    }

    return move_list;
}

/* note: limited handling of ambiguities */
bool move_to_san(move_st *list, const move_st *p_move, char *san_buf, uint8_t buf_sz)
{
    move_st *similar = NULL;
    char *san = san_buf;

    move_st *elt;
    LL_FOREACH(list, elt) {
        if ((PAWN != PIECE_TYPE(p_move->piece)) && (p_move->piece == elt->piece) &&
            (p_move->to == elt->to) && (p_move->from != elt->from)) {
            if (NULL != similar) {
                // to do
                LOGW("too many ambiguities");
            }
            similar = elt;
        }
    }

    if (p_move->flags & BIT_KSIDE_CASTLE)
    {
        strncpy(san_buf, "O-O", buf_sz);
    }
    else if (p_move->flags & BIT_QSIDE_CASTLE)
    {
        strncpy(san_buf, "O-O-O", buf_sz);
    }
    else if (NULL == similar)
    {
        if (PAWN == PIECE_TYPE(p_move->piece))
        {
            if (p_move->flags & (BIT_CAPTURE | BIT_EP_CAPTURE)) {
                san += snprintf(san, buf_sz, "%cx", FILE(p_move->from) + 'a');
            }
            san += snprintf(san, buf_sz - 2, "%c%u", ALGEBRAIC(p_move->to));
        }
        else // officials
        {
            *san++ = toupper(PIECE_TYPE(p_move->piece));
            if (p_move->flags & BIT_CAPTURE) {
                *san++ = 'x';
            }
            snprintf(san, buf_sz - 2, "%c%u", ALGEBRAIC(p_move->to));
        }
    }
    else
    {
        *san++ = toupper(PIECE_TYPE(p_move->piece));
        if (FILE(p_move->from) != FILE(similar->from)) {
            *san++ = 'a' + FILE(p_move->from);
        } else if (RANK(p_move->from) != RANK(similar->from)) {
            *san++ = '0' + 8 - RANK(p_move->from);
        } else {
            *san++ = 'a' + FILE(p_move->from);
            *san++ = '0' + 8 - RANK(p_move->from);
        }

        san += snprintf(san, buf_sz - 4, "%c%u", ALGEBRAIC(p_move->to));
    }

    if (p_move->flags & BIT_PROMOTION) {
        *san++ = '=';
        *san++ = toupper(PIECE_TYPE(p_move->promoted));
    }

    return true;
}

bool find_move(game_st *p_game /*board*/, move_st *list /*moves list*/, const uint8_t *scan /*input or desired raw position*/, move_st *p_move /*found move*/)
{
    bool found = false;

    if ((NULL != p_game) && (NULL != list) && (NULL != scan) && (NULL != p_move))
    {
        uint8_t *board = p_game->board;
        move_st *elt;
        LL_FOREACH(list, elt) {

            bool different = false;
            make_move(p_game, elt);
            for (uint8_t idx = 0; idx < 64; idx++) {
                uint8_t sq = IDX_TO_SQUARE(idx);
                if (scan[idx] == board[sq]) {
                    // same
                } else if ((elt->flags & BIT_PROMOTION) && (sq == elt->to) &&
                           (_NONE != PIECE_TYPE(scan[idx])) && (PAWN != PIECE_TYPE(scan[idx]))) {
                    //LOGD("promote at %c%u %02x %02x", ALGEBRAIC(sq), scan[idx], board[sq]);
                } else {
                    //LOGD("different at %c%u %02x %02x (%02x)", ALGEBRAIC(sq), scan[idx], board[sq], elt->flags);
                    different = true;
                    break;
                }
            }
            undo_move(p_game);

            if (!different) {
                //LOGD("found move %c %c%u-%c%u (%02x)", elt->piece, ALGEBRAIC(elt->from), ALGEBRAIC(elt->to), elt->flags);
                memcpy(p_move, elt, sizeof(move_st));
                found = true;
                break;
            }
        }
    }

    return found;
}

uint8_t hint_moves(game_st *p_game /*board*/, move_st *list /*moves list*/, const uint8_t *scan, uint8_t *squares_buf, uint8_t max_count)
{
    uint8_t count = 0;
    uint8_t u8_toggle_idx = 0;
    uint8_t sq = 0;

    if ((NULL != p_game) && (NULL != list) && (NULL != scan) && (NULL != squares_buf))
    {
        for (uint8_t idx = 0; idx < 64; idx++) {
            sq = IDX_TO_SQUARE(idx);
            if (p_game->board[sq] != scan[idx]) {
                //LOGD("different at %c%u %02x %02x", ALGEBRAIC(sq), scan[idx], p_game->board[sq]);
                count++;
                u8_toggle_idx = idx;
            }
        }

        if (1 != count) {
            // too many changes
            count = 0;
        } else if (0 != scan[u8_toggle_idx]) {
            sq = IDX_TO_SQUARE(u8_toggle_idx);
            LOGD("new piece %c on %c%u", scan[u8_toggle_idx], ALGEBRAIC(sq));
            count = 0;
        } else {
            uint8_t rank = u8_toggle_idx >> 3;
            uint8_t file = u8_toggle_idx & 7;
            uint8_t from = ((7 - rank) << 4) + file;

            count = 0;
            squares_buf[count++] = from;
            move_st *elt;
            LL_FOREACH(list, elt) {
                if ((elt->from == from) && (count < max_count)) {
                    squares_buf[count++] = elt->to;
                }
            }
        }

    }

    return count;
}

void clear_moves(move_st **plist)
{
    if ((NULL != plist) && (NULL != *plist))
    {
        move_st *elt, *tmp;
        LL_FOREACH_SAFE(*plist, elt, tmp) {
            //LOGD("del %02x from %d to %d", elt->piece, elt->from, elt->to);
            LL_DELETE(*plist, elt);
            free(elt);
        }
    }
}

} // namespace chess
