
#include "globals.h"
#include "mfrc522/mfrc522.h"

#include "chess/chess.h"
#include "ui/ui.h"
#include "board.h"


namespace brd
{

static enum {
    BRD_STATE_INIT,
    BRD_STATE_SCAN,
    BRD_STATE_IDLE
} e_state;

static MFRC522      rc522(fspi_transfer, PIN_RFID_RST);

static uint8_t      au8_pieces[64];
static uint32_t     au32_toggle_ms[64];

static uint8_t      u8_selected_file;
static uint8_t      u8_selected_rank;


// set column
static inline void select_file(uint8_t file)
{
    u8_selected_file = file;
    PIN_WRITE(RFID_CS_A, file & 1 ? 1 : 0);
    PIN_WRITE(RFID_CS_B, file & 2 ? 1 : 0);
    PIN_WRITE(RFID_CS_C, file & 4 ? 1 : 0);
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
        chess::init();
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
            //uint32_t ms_start = millis();
            chess::loop(scan());
            //LOGD("scan duration %lu ms", millis() - ms_start);
        }

        break;

    default:
        e_state = BRD_STATE_INIT;
        break;
    }
}

const uint8_t *pu8_pieces(void)
{
    return au8_pieces;
}

const uint32_t *pu32_toggle_ms(void)
{
    return au32_toggle_ms;
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
    static const uint8_t expected_rxgain = 0x40; // default RxGain is 0x40 (33dB)
    bool b_complete = true;

    for (uint8_t rank = RANK_START; b_complete && (rank <= RANK_END); rank++)
    {
        select_rank(rank);
        rc522.PCF_HardReset();

        ui::leds::clear();

        for (uint8_t file = FILE_START; b_complete && (file <= FILE_END); file++)
        {
            select_file(file);

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

static inline void square_init(void)
{
#if 0
    rc522.PCD_Init();
#else
    // wait for 150ms to be ready
    uint32_t    ms_timeout  = millis() + 150;
    uint8_t     u8_cmdreg   = 0;

    // do soft-reset
    rc522.PCD_WriteRegister(MFRC522::CommandReg, MFRC522::PCD_SoftReset);
    do {
        delayms(1);
        u8_cmdreg = rc522.PCD_ReadRegister(MFRC522::CommandReg);
        if (0 == (u8_cmdreg & 0x10)) { // check if powerdown bit is cleared
            break; // reader is now ready
        }
    } while (millis() < ms_timeout);

    if (0 != (u8_cmdreg & 10)) {
        LOGW("square %c%u not ready (cmdreg 0x%02x)", 'a' + u8_selected_file, u8_selected_rank + 1, u8_cmdreg);
    }

    rc522.PCD_WriteRegister(MFRC522::TModeReg, 0x80);
    rc522.PCD_WriteRegister(MFRC522::TPrescalerReg, 0xA9);
    rc522.PCD_WriteRegister(MFRC522::TReloadRegH, 0x03);
    rc522.PCD_WriteRegister(MFRC522::TReloadRegL, 0xE8);

    rc522.PCD_WriteRegister(MFRC522::TxASKReg, 0x40);
    rc522.PCD_WriteRegister(MFRC522::ModeReg, 0x3D);
    rc522.PCD_AntennaOn();
#endif
}

static inline void square_deinit(void)
{
#if 0
    rc522.PICC_HaltA();
#else // power-down & receiver-off
    rc522.PCD_WriteRegister(MFRC522::CommandReg, 0x30);
#endif
}


static inline bool has_piece(void)
{
    MFRC522::StatusCode result;
    uint8_t             bufferATQA[2];
    uint8_t             bufferSize = sizeof(bufferATQA);

    // Reset baud rates
    rc522.PCD_WriteRegister(MFRC522::TxModeReg, 0x00);
    rc522.PCD_WriteRegister(MFRC522::RxModeReg, 0x00);
    // Reset ModWidthReg
    rc522.PCD_WriteRegister(MFRC522::ModWidthReg, 0x26);

    result = rc522.PICC_RequestA(bufferATQA, &bufferSize);
    return ((MFRC522::STATUS_OK == result) || (MFRC522::STATUS_COLLISION == result));
}

static inline uint8_t read_piece(uint16_t u8_expected_piece, uint8_t u8_retry, bool b_init)
{
    MFRC522::StatusCode status;
    uint8_t             buffer[16 + 2 /*crc*/]; // minimum
    uint8_t             size;

    if (b_init)
    {
        //rc522.PCD_Reset(); // soft reset
        square_init();
    }

    if (!has_piece())
    {
        if (0 != u8_expected_piece)
        {
            if (u8_retry > 0)
            {
                u8_retry--;
                return read_piece(u8_expected_piece, u8_retry, u8_retry & 1);
            }
            else
            {
                //LOGD("removed %c on %c%u?", u8_expected_piece, 'a' + u8_selected_file, u8_selected_rank + 1);
            }
        }
    }
    else if ((16 > (size = sizeof(buffer))) || (MFRC522::STATUS_OK != (status = rc522.MIFARE_Read(0, buffer, &size))) ||
             (16 > (size = sizeof(buffer))) || (MFRC522::STATUS_OK != (status = rc522.MIFARE_Read(NTAG_DATA_START_PAGE, buffer, &size))))
    {
        if (u8_retry > 0)
        //if ((u8_retry > 0) && ((MFRC522::STATUS_TIMEOUT==status) || (MFRC522::STATUS_CRC_WRONG==status)))
        {
            return read_piece(u8_expected_piece, --u8_retry, false);
        }
        else
        {
            LOGW("read failed on %c%u (status=%d)", 'a' + u8_selected_file, u8_selected_rank + 1, status);
        }
    }
    else
    {
        uint8_t u8_piece = buffer[NTAG_DATA_PIECE_OFFSET];
        uint8_t u7_type  = PIECE_TYPE(u8_piece);
        //uint8_t b_color  = PIECE_COLOR(u8_piece);
        if (VALID_PIECE(u7_type))
        {
            //LOGD("[%c on %c%u] %s %s", u8_piece, 'a' + u8_selected_file, u8_selected_rank + 1,
            //    chess::color_to_string(b_color), chess::piece_to_string(u7_type));
            return u8_piece;
        }
        else
        {
            //LOGW("invalid piece %02x on %c%u", u8_piece, 'a' + u8_selected_file, u8_selected_rank + 1);
        }
    }

    return 0;
}

static uint32_t scan(void)
{
    static const uint8_t MAX_READ_RETRIES = 16;
    static uint32_t ms_last_toggle = 0;

    for (uint8_t rank = 0; rank < 8; rank++)
    {
        select_rank(rank);
        rc522.PCF_HardReset();

        for (uint8_t file = 0; file < 8; file++)
        {
            select_file(file);

            uint8_t idx   = (rank<<3) + file;
            uint8_t piece = read_piece(au8_pieces[idx], MAX_READ_RETRIES, true);

            if (au8_pieces[idx] != piece)
            {
                uint8_t piece_check = read_piece(au8_pieces[idx], MAX_READ_RETRIES, true);

                if (piece != piece_check) // re-read
                {
                    //LOGD("re-check %02x vs %02x on %c%u", piece, piece_check, 'a' + file, rank + 1);
                    piece_check = read_piece(piece, MAX_READ_RETRIES, true);
                }
                if ((piece != piece_check) && (au8_pieces[idx] != piece_check)) // verify x2
                {
                    LOGW("verify failed %02x vs %02x on %c%u", piece, piece_check, 'a' + file, rank + 1);
                }
                piece = piece_check; // ignore further errors
                if (au8_pieces[idx] != piece)
                {
                    au32_toggle_ms[idx] = millis();
                    //LOGD("toggle %c on %c%u", piece ? piece : '-', 'a' + file, rank + 1);
                    ms_last_toggle = au32_toggle_ms[idx];
                }
            }

            au8_pieces[idx] = piece;

            square_deinit();
        }

    }
    //LOGD("scan done");
    return ms_last_toggle;
}

} // namespace brd
