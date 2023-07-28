
#include "globals.h"
#include "board/board.h"
#include "ui/ui.h"
#include "chess.h"


namespace chess
{

static const uint8_t   *pu8_pieces = NULL;
static uint8_t          au8_prev_pieces[64];

static bool             skip_start_fen = false;

SemaphoreHandle_t       mtx = NULL;

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


void init(void)
{
    if (NULL == mtx)
    {
        mtx = xSemaphoreCreateMutex();
        ASSERT((NULL != mtx), "chess mutex allocation failed");

        pu8_pieces = brd::pu8_pieces();
    }

}

static inline bool check_start_fen(void)
{
    uint8_t u8_diff = 0;

    ui::leds::clear();

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

    if (false == b_start_fen)
    {
        b_start_fen = skip_start_fen || check_start_fen();
        if (b_start_fen)
        {
            memcpy(au8_prev_pieces, pu8_pieces, sizeof(au8_prev_pieces));
        }
    }
    else if (0 == memcmp(au8_prev_pieces, pu8_pieces, sizeof(au8_prev_pieces)))
    {
        //LOGD("no change yet");
    }
    else if (millis() - u32_last_checked < 500)
    {
        //LOGD("blanking");
    }
    else
    {
        u32_last_checked = millis();
    }
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
