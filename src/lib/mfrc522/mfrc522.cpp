
#include "globals.h"

#include "mfrc522.h"

MFRC522::MFRC522(xfer_func_t xfer, gpio_num_t rst): _xfer(xfer), _rst(rst)
{
    memset(&uid, 0, sizeof(uid));
}

void MFRC522::PCD_WriteRegister(PCD_Register reg, uint8_t value)
{
    tx_buf[0] = reg; // address: MSB == 0 is for writing
    tx_buf[1] = value;
    _xfer(tx_buf, NULL, 2);
}

void MFRC522::PCD_WriteRegister(PCD_Register reg, uint8_t count, uint8_t *values)
{
    memset(tx_buf, 0, sizeof(tx_buf));
    tx_buf[0] = reg;  // address: MSB == 0 is for writing
    if (count >= sizeof(tx_buf)) {
        count = sizeof(tx_buf) - 1;
    }
    memcpy(&tx_buf[1], values, count);
    _xfer(tx_buf, NULL, 1 + count);
}

uint8_t MFRC522::PCD_ReadRegister(PCD_Register reg)
{
    tx_buf[0] = 0x80 | reg; // address: MSB == 1 is for reading
    tx_buf[1] = 0x00;
    _xfer(tx_buf, rx_buf, 2);

    return rx_buf[1]; // byte after write address
}

void MFRC522::PCD_ReadRegister(PCD_Register reg, uint8_t count, uint8_t *values, uint8_t rxAlign)
{
    //LOGD("%s(%x, %u, %p, %u)", __func__, reg>>1, count, values, rxAlign);
    if (count > 0)
    {
        uint8_t address = 0x80 | reg; // address: MSB == 1 is for reading
        memset(tx_buf, address, sizeof(tx_buf));
        if (count >= sizeof(tx_buf)) {
            count = sizeof(tx_buf) - 1;
        }
        tx_buf[count] = 0x00; // Read the final byte. Send 0 to stop reading.
        _xfer(tx_buf, rx_buf, 1 + count);
        uint8_t *prx = &rx_buf[1];

        if (rxAlign) {
            // Create bit mask for bit positions rxAlign..7
            uint8_t mask = (0xFF << rxAlign) & 0xFF;
            // Apply mask to both current value of values[0] and the new data in value.
            values[0] = (values[0] & ~mask) | ((*prx++) & mask);
            values++;
            count--;
        }
        memcpy(values, prx, count);
    }
}

void MFRC522::PCD_SetRegisterBitMask(PCD_Register reg, uint8_t mask)
{
    uint8_t tmp = PCD_ReadRegister(reg);
    PCD_WriteRegister(reg, tmp | mask);     // set bit mask
}

void MFRC522::PCD_ClearRegisterBitMask(PCD_Register reg, uint8_t mask)
{
    uint8_t tmp = PCD_ReadRegister(reg);
    PCD_WriteRegister(reg, tmp & (~mask));  // clear bit mask
}

/*
local crc calculation
https://stackoverflow.com/questions/40869293/iso-iec-14443a-crc-calcuation
*/
void MFRC522::CalcCRC(uint8_t *data, uint8_t length, uint8_t *result)
{
    uint32_t wCrc = 0x6363;

    do {
        uint8_t  bt;
        bt = *data++;
        bt = (bt ^ (uint8_t)(wCrc & 0x00FF));
        bt = (bt ^ (bt << 4));
        wCrc = (wCrc >> 8) ^ ((uint32_t) bt << 8) ^ ((uint32_t) bt << 3) ^ ((uint32_t) bt >> 4);
    } while (--length);

    *result++ = (uint8_t)(wCrc & 0xFF);
    *result = (uint8_t)((wCrc >> 8) & 0xFF);
}

void MFRC522::PCF_HardReset()
{
    gpio_set_level(PIN_RFID_RST, 0);
    delayms(1); // at least 100ns
    gpio_set_level(PIN_RFID_RST, 1);
    delayms(1); // at least 38us
}

void MFRC522::PCD_Init()
{
    // Reset baud rates
    PCD_WriteRegister(TxModeReg, 0x00);
    PCD_WriteRegister(RxModeReg, 0x00);
    // Reset ModWidthReg
    PCD_WriteRegister(ModWidthReg, 0x26);

    // When communicating with a PICC we need a timeout if something goes wrong.
    // f_timer = 13.56 MHz / (2*TPreScaler+1) where TPreScaler = [TPrescaler_Hi:TPrescaler_Lo].
    // TPrescaler_Hi are the four low bits in TModeReg. TPrescaler_Lo is TPrescalerReg.
    PCD_WriteRegister(TModeReg, 0x80);          // TAuto=1; timer starts automatically at the end of the transmission in all communication modes at all speeds
    PCD_WriteRegister(TPrescalerReg, 0xA9);     // TPreScaler = TModeReg[3..0]:TPrescalerReg, ie 0x0A9 = 169 => f_timer=40kHz, ie a timer period of 25μs.
    PCD_WriteRegister(TReloadRegH, 0x03);       // Reload timer with 0x3E8 = 1000, ie 25ms before timeout.
    PCD_WriteRegister(TReloadRegL, 0xE8);

    PCD_WriteRegister(TxASKReg, 0x40);          // Default 0x00. Force a 100 % ASK modulation independent of the ModGsPReg register setting
    PCD_WriteRegister(ModeReg, 0x3D);           // Default 0x3F. Set the preset value for the CRC coprocessor for the CalcCRC command to 0x6363 (ISO 14443-3 part 6.2.4)
    PCD_AntennaOn();                            // Enable the antenna driver pins TX1 and TX2 (they were disabled by the reset)
}

void MFRC522::PCD_AntennaOn()
{
    uint8_t value = PCD_ReadRegister(TxControlReg);
    if ((value & 0x03) != 0x03) {
        PCD_WriteRegister(TxControlReg, value | 0x03);
    }
}
