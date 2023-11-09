
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

//static uint8_t      au8_pieces[64];
//static uint32_t     au32_toggle_ms[64];

static uint8_t      u8_selected_file;
static uint8_t      u8_selected_rank;


// set column
static inline void select_file(uint8_t file)
{
    u8_selected_file = file;
    PIN_WRITE(RFID_SS_A, file & 1 ? 1 : 0);
    PIN_WRITE(RFID_SS_B, file & 2 ? 1 : 0);
    PIN_WRITE(RFID_SS_C, file & 4 ? 1 : 0);
}

// set row
static inline void select_rank(uint8_t rank)
{
    u8_selected_rank = rank;
    PIN_WRITE(RFID_RST_A, rank & 1 ? 1 : 0);
    PIN_WRITE(RFID_RST_B, rank & 2 ? 1 : 0);
    PIN_WRITE(RFID_RST_C, rank & 4 ? 1 : 0);
}


static bool checkSquares(void);
static void animate_squares(void);
static uint32_t scan(void);

bool init()
{
    //LOGD("%s()", __func__);

    hal_fspi_init();

    select_rank(0);
    select_file(0);

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
        if (MAIN_BTN.pressedDuration() >= 3000UL)
        {
            LOGD("restart");
            MAIN_BTN.resetCount();
            e_state = BRD_STATE_INIT;
        }
        else
        {
            // to do
            scan();
        }

        break;

    default:
        e_state = BRD_STATE_INIT;
        break;
    }
}

static bool checkSquares(void)
{
#if 1 // all 64 squares
    static const uint8_t RANK_START = 0;
    static const uint8_t RANK_END   = 7;
    static const uint8_t FILE_START = 0;
    static const uint8_t FILE_END   = 7;
#else // testing only
    static const uint8_t RANK_START = 0;
    static const uint8_t RANK_END   = 3;
    static const uint8_t FILE_START = 0;
    static const uint8_t FILE_END   = 3;
#endif
    //static const uint8_t expected_rxgain = 0x40; // default RxGain is 0x40 (33dB)
    bool b_complete = true;

    for (uint8_t rank = RANK_START; b_complete && (rank <= RANK_END); rank++)
    {
        select_rank(rank);
        // to do: rc522.PCF_HardReset();

        ui::leds::clear();

        for (uint8_t file = FILE_START; b_complete && (file <= FILE_END); file++)
        {
            select_file(file);
#if 0 // to do
            rc522.PCD_Init();
            delayms(1);

            uint8_t u8_version = rc522.PCD_ReadRegister(MFRC522::VersionReg);
            if ((u8_version == 0x00) || (u8_version == 0xFF))
            {
                LOGW("sensor not found on square %c%u (%02X)", 'a' + file, rank + 1, u8_version);
                DISPLAY_CLEAR_ROW(20, 8);
                DISPLAY_TEXT1(0, 20, "sensor error on %c%u", 'A' + file, rank + 1);
                ui::leds::clear();
                ui::leds::setColor(rank, file, ui::leds::LED_RED);
                b_complete = false;
                break;
            }

            uint8_t rxgain = rc522.PCD_GetAntennaGain();
            if (expected_rxgain != rxgain) // defective reader chip?
            {
                LOGW("rxgain = 0x%02x on %c%u", rxgain, 'a' + u8_selected_file, u8_selected_rank + 1);
                rc522.PCD_WriteRegister(MFRC522::RFCfgReg, expected_rxgain);
            }
#endif
            //LOGD("found %c%u v%02X", 'a' + file, rank + 1, u8_version);
            ui::leds::setColor(rank, file, ui::leds::LED_GREEN);
        }

        ui::leds::update();
        delayms(20);
    }

    return b_complete;
}


static void _lit_square(uint8_t u8_rank, uint8_t u8_file)
{
    ui::leds::clear();
    ui::leds::setColor(u8_rank, u8_file, ui::leds::LED_ORANGE);
    ui::leds::update();
    delayms(5);
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
    delayms(250);
    LED_OFF();
    delayms(250);

    return 0;
}

} // namespace brd
