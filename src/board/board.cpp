
#include <MFRC522.h>

#include "globals.h"

#include "chess/chess.h"
#include "ui/ui.h"

#include "board.h"

namespace brd
{

SPIClass            spiRFID(RFID_SPI_BUS);
MFRC522             rc522(spiRFID, RFID_SS_PIN, RFID_RST_PIN, true, false);  // Create MFRC522 instance

static enum {
    BRD_STATE_INIT,
    BRD_STATE_SCAN,
    BRD_STATE_IDLE
} e_state;

static uint8_t      au8_pieces[64];
static uint32_t     au32_toggle_ms[64];

static uint8_t      u8_selected_file;
static uint8_t      u8_selected_rank;


// set column
static inline void select_file(uint8_t file)
{
    u8_selected_file = file;
    digitalWrite(RFID_SS_A_PIN, file & 1 ? HIGH : LOW);
    digitalWrite(RFID_SS_B_PIN, file & 2 ? HIGH : LOW);
    digitalWrite(RFID_SS_C_PIN, file & 4 ? HIGH : LOW);
}

// set row
static inline void select_rank(uint8_t rank)
{
    u8_selected_rank = rank;
    digitalWrite(RFID_RST_A_PIN, rank & 1 ? HIGH : LOW);
    digitalWrite(RFID_RST_B_PIN, rank & 2 ? HIGH : LOW);
    digitalWrite(RFID_RST_C_PIN, rank & 4 ? HIGH : LOW);
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
    static const uint8_t RANK_END   = 7;
    static const uint8_t FILE_START = 0;
    static const uint8_t FILE_END   = 3;
#endif
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
            delay(1);

            uint8_t u8_version = rc522.PCD_ReadRegister(MFRC522::VersionReg);
            if ((u8_version == 0x00) || (u8_version == 0xFF))
            {
                LOGW("sensor not found on square %c%u (%02X)", 'a' + file, rank + 1, u8_version);
                ui::leds::clear();
                ui::leds::setColor(rank, file, ui::leds::LED_RED);
                b_complete = false;
            }
            else
            {
                //LOGD("found %c%u v%02X", 'a' + file, rank + 1, u8_version);
                ui::leds::setColor(rank, file, ui::leds::LED_GREEN);
            }
        }

        ui::leds::update();
        delay(20);
    }

    return b_complete;
}

static void lit_square(uint8_t u8_rank, uint8_t u8_file)
{
    ui::leds::clear();
    ui::leds::setColor(u8_rank, u8_file, ui::leds::LED_ORANGE);
    ui::leds::update();
    delay(5);
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
        while (file <= file_end)  { lit_square(rank_start, file++);  }

        rank = rank_start;
        while (rank <= rank_end)   { lit_square(rank++, file_end);   }

        file = file_end;
        while (file >= file_start) { lit_square(rank_end, file--);   }

        rank = rank_end;
        while (rank >= rank_start)  { lit_square(rank--, file_start); }

        file_start++;
        file_end--;
        rank_start++;
        rank_end--;
    }
}

static inline uint8_t read_piece(void)
{
    byte buffer[16 + 2]; // minimum
    byte size = sizeof(buffer);

    MFRC522::StatusCode status = rc522.MIFARE_Read(NTAG_DATA_START_PAGE, buffer, &size);
    //LOGD("read %c%u = %d", 'a' + u8_selected_file, u8_selected_rank + 1, status);
    if (MFRC522::STATUS_OK != status)
    {
        LOGW("read failed on %c%u (status=%d)", 'a' + u8_selected_file, u8_selected_rank + 1, status);
    }
    else
    {
        uint8_t u8_piece = buffer[NTAG_DATA_PIECE_OFFSET];
        uint8_t u7_type  = PIECE_TYPE(u8_piece);
        uint8_t b_color  = PIECE_COLOR(u8_piece);
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

static void scan(void)
{
    for (uint8_t rank = 0; rank < 8; rank++)
    {
        select_rank(rank);
        rc522.PCF_HardReset();
#if 0
        delay(2);
        for (uint8_t file = 0; file < 8; file++)
        {
            select_file(file);
            rc522.PCD_Init();
        }

        for (uint8_t file = 0; file < 8; file++)
        {
            select_file(file);

            uint8_t piece = 0x00;
            ui::leds::setColor(rank, file, ui::leds::LED_OFF);
            if (rc522.PICC_IsNewCardPresent()) {
                piece = read_piece();
                ui::leds::setColor(rank, file, ui::leds::LED_ORANGE);
            }

            (void)rc522.PICC_HaltA();
        }
#elif 1
        for (uint8_t file = 0; file < 8; file++)
        {
            select_file(file);
            rc522.PCD_Init();

            uint8_t idx   = (rank<<3) + file;
            uint8_t piece = rc522.PICC_IsNewCardPresent() ? read_piece() : 0x00;

            if (au8_pieces[idx] != piece)
            {
              #if 0
                rc522.PCD_Reset(); // soft reset
                rc522.PCD_Init();
                uint8_t piece_check = rc522.PICC_IsNewCardPresent() ? read_piece() : 0x00;
              #else
                uint8_t piece_check = read_piece();
              #endif
                if (piece != piece_check) // re-read
                {
                    //LOGD("re-check %02x vs %02x on %c%u", piece, piece_check, 'a' + file, rank + 1);
                    rc522.PCD_Reset(); // soft reset
                    rc522.PCD_Init();
                    piece_check = rc522.PICC_IsNewCardPresent() ? read_piece() : 0x00;
                }
                if ((piece != piece_check) && (au8_pieces[idx] != piece_check)) // verify x2
                {
                    LOGW("verify failed %02x vs %02x on %c%u", piece, piece_check, 'a' + file, rank + 1);
                }
                piece = piece_check; // ignore further errors
                if (au8_pieces[idx] != piece)
                {
                    au32_toggle_ms[idx] = millis();
                }
            }

            au8_pieces[idx] = piece;

            (void)rc522.PICC_HaltA();
        }
#endif
    }
    //LOGD("scan done");
    ui::leds::update();
}

static void taskBoard(void *)
{
    for(;;)
    {
        switch (e_state)
        {
        case BRD_STATE_INIT:
            if (checkSquares() && checkSquares()) // check 2x
            {
                animate_squares();
                scan(); // initial scan
                chess::init();
                e_state = BRD_STATE_SCAN;
            }
            else
            {
                delay(3000);
            }
            break;

        case BRD_STATE_SCAN:
            if (ui::btn::pb1.pressedDuration() > 5000UL)
            {
                e_state = BRD_STATE_INIT;
            }
            else
            {
                //uint32_t ms_start = millis();
                scan();
                //LOGD("scan duration %lu ms", millis() - ms_start);
                chess::loop();
            }
            break;

        default:
            e_state = BRD_STATE_INIT;
            break;
        }

        delay(2);
    }
}

void init(void)
{
    memset(au8_pieces, 0, sizeof(au8_pieces));
    memset(au32_toggle_ms, 0, sizeof(au32_toggle_ms));

    pinMode(RFID_SS_A_PIN, OUTPUT);
    pinMode(RFID_SS_B_PIN, OUTPUT);
    pinMode(RFID_SS_C_PIN, OUTPUT);
  //pinMode(RFID_SS_PIN, OUTPUT);

    pinMode(RFID_RST_A_PIN, OUTPUT);
    pinMode(RFID_RST_B_PIN, OUTPUT);
    pinMode(RFID_RST_C_PIN, OUTPUT);
  //pinMode(RFID_RST_PIN, OUTPUT);

    select_rank(0);
    select_file(0);

  //spiRFID.setFrequency(1000000);
    spiRFID.begin(RFID_SCK_PIN, RFID_MISO_PIN, RFID_MOSI_PIN, RFID_SS_PIN);

    e_state = BRD_STATE_INIT;

    xTaskCreate(
        taskBoard,      /* Task function. */
        "taskBoard",    /* String with name of task. */
        16*1024,        /* Stack size in bytes. */
        NULL,           /* Parameter passed as input of the task */
        3,              /* Priority of the task. */
        NULL);          /* Task handle. */
}

const uint8_t *pu8_pieces(void)
{
    return au8_pieces;
}

const uint32_t *pu32_toggle_ms(void)
{
    return au32_toggle_ms;
}

} // namespace brd
