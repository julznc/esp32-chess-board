
#include <MFRC522.h>

#include "globals.h"
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
#if 0 // all 64 squares
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
    bool b_complete = true;

    for (uint8_t rank = RANK_START; b_complete && (rank <= RANK_END); rank++)
    {
        select_rank(rank);
        rc522.PCF_HardReset();

        for (uint8_t file = FILE_START; b_complete && (file <= FILE_END); file++)
        {
            select_file(file);
            rc522.PCD_Init();
            delay(1);

            uint8_t u8_version = rc522.PCD_ReadRegister(MFRC522::VersionReg);
            if ((u8_version == 0x00) || (u8_version == 0xFF))
            {
                LOGW("sensor not found on square %c%u (%02X)", 'a' + file, rank + 1, u8_version);
                b_complete = false;
                break;
            }
            LOGD("found %c%u v%02X", 'a' + file, rank + 1, u8_version);
        }
    }

    return b_complete;
}

static inline uint8_t read_piece(void)
{
    MFRC522::StatusCode status;
    uint8_t u8_piece = 0;

    byte buffer[16 + 2]; // minimum
    byte size = sizeof(buffer);

    status = rc522.MIFARE_Read(0x04 /*NTAG_PIECE_ADDRESS*/, buffer, &size);
    LOGD("read %c%u = %d", 'a' + u8_selected_file, u8_selected_rank + 1, status);
    if (MFRC522::STATUS_OK != status)
    {
        LOGW("read failed on %c%u", 'a' + u8_selected_file, u8_selected_rank + 1);
    }
    else
    {
        dump_bytes(buffer, size);
        // to do
    }

    return u8_piece;
}

static void scan(void)
{
    for (uint8_t rank = 0; rank < 8; rank++)
    {
        select_rank(rank);
        rc522.PCF_HardReset();
#if 1
        delay(2);
        for (uint8_t file = 0; file < 8; file++)
        {
            select_file(file);
            rc522.PCD_Init();
        }

        delay(1);
        for (uint8_t file = 0; file < 8; file++)
        {
            select_file(file);

            uint8_t piece = 0x00;
            ui::leds::setColor(rank, file, 0, 0, 0);
            if (rc522.PICC_IsNewCardPresent()) {
                piece = read_piece();
                ui::leds::setColor(rank, file, rand(), rand(), rand());
            }

            (void)rc522.PICC_HaltA();
        }

#else
        for (uint8_t file = 0; file < 8; file++)
        {
            select_file(file);
            rc522.PCD_Init();
            delay(1);

            uint8_t idx = (rank<<3) + file;
            uint8_t piece = 0x00;
            //uint8_t piece = rc522.PICC_IsNewCardPresent() ? read_piece() : 0x00;
            ui::leds::setColor(rank, file, 0, 0, 0);
            if (rc522.PICC_IsNewCardPresent()) {
                piece = read_piece();
                ui::leds::setColor(rank, file, rand(), rand(), rand());
            }

            //lock();
            if (au8_pieces[idx] != piece)
            {
                //LOGD("verify %02x vs %02x on %c%u", piece, au8_pieces[idx], 'a' + file, rank + 1);
                rc522.PCD_Reset(); // soft reset
                rc522.PCD_Init();
                uint8_t piece_check = rc522.PICC_IsNewCardPresent() ? read_piece() : 0x00;
                if (piece != piece_check) // re-read
                {
                    //LOGD("re-check %02x vs %02x on %c%u", piece, piece_check, 'a' + file, rank + 1);
                    rc522.PCD_Reset(); // soft reset
                    rc522.PCD_Init();
                    piece_check = rc522.PICC_IsNewCardPresent() ? read_piece() : 0x00;
                }
                //if (piece != piece_check) // verify x2
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
            //unlock();
            if (MFRC522::STATUS_OK != rc522.PICC_HaltA())
            {
                //LOGW("halt error?");
            }
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
            if (checkSquares())
            {
                e_state = BRD_STATE_SCAN;
            }
            else
            {
                delay(3000);
            }
            break;

        case BRD_STATE_SCAN:
            //LOGD("start scan");
            scan();
            //LOGD("end scan");
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

} // namespace brd
