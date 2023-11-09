
#include "globals.h"
#include "ui/ui.h"
#include "board.h"


namespace brd
{

static enum {
    BRD_STATE_INIT,
    BRD_STATE_SCAN,
    BRD_STATE_IDLE
} e_state;

static bool checkSquares(void);
static void animate_squares(void);
static uint32_t scan(void);

bool init()
{
    LOGD("%s()", __func__);

    e_state = BRD_STATE_INIT;

    return true;
}

void loop()
{
    switch (e_state)
    {
    case BRD_STATE_INIT:
        if (checkSquares() && checkSquares()) // check 2x
        {
            animate_squares();
            scan(); // initial scan
            e_state = BRD_STATE_SCAN;
        }
        else
        {
            delayms(10000);
        }
        break;

    case BRD_STATE_SCAN:
        scan();
        break;

    default:
        e_state = BRD_STATE_INIT;
        break;
    }
}

static bool checkSquares(void)
{
    // to do
    return true;
}


static void _lit_square(uint8_t u8_rank, uint8_t u8_file)
{
    ui::leds::clear();
    ui::leds::setColor(u8_rank, u8_file, ui::leds::LED_ORANGE);
    ui::leds::update();
    delayms(50);
}

static void animate_squares(void)
{
    int8_t rank, file;
    int8_t file_start  = 0;
    int8_t rank_start  = 0;
    int8_t file_end    = 7;
    int8_t rank_end    = 7;

    while ((file_start <= file_end))
    {
        file = file_start;
        while (file <= file_end)  { _lit_square(rank_start, file++);  }

        rank = rank_start;
        while (rank <= rank_end)   { _lit_square(rank++, file_end);   }

        file = file_end;
        while (file >= file_start) { _lit_square(rank_end, file--);   }

        rank = rank_end;
        while (rank >= rank_start)  { _lit_square(rank--, file_start); }

        file_start++;
        file_end--;
        rank_start++;
        rank_end--;
    }
}

static uint32_t scan(void)
{
    // to do

    LED_ON();
    delayms(500);
    LED_OFF();
    delayms(500);

    return 0;
}

} // namespace brd
