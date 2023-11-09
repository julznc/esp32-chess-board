
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

uint8_t MFRC522::PCD_GetAntennaGain()
{
    return PCD_ReadRegister(RFCfgReg) & (0x07<<4);
}


MFRC522::StatusCode MFRC522::PCD_TransceiveData(uint8_t *sendData,      ///< Pointer to the data to transfer to the FIFO.
                                                uint8_t sendLen,        ///< Number of bytes to transfer to the FIFO.
                                                uint8_t *backData,      ///< nullptr or pointer to buffer if data should be read back after executing the command.
                                                uint8_t *backLen,       ///< In: Max number of bytes to write to *backData. Out: The number of bytes returned.
                                                uint8_t *validBits,     ///< In/Out: The number of valid bits in the last byte. 0 for 8 valid bits. Default nullptr.
                                                uint8_t rxAlign,        ///< In: Defines the bit position in backData[0] for the first bit received. Default 0.
                                                bool checkCRC )         ///< In: True => The last two bytes of the response is assumed to be a CRC_A that must be validated.
{
    uint8_t waitIRq = 0x30;     // RxIRq and IdleIRq
    return PCD_CommunicateWithPICC(PCD_Transceive, waitIRq, sendData, sendLen, backData, backLen, validBits, rxAlign, checkCRC);
}

MFRC522::StatusCode MFRC522::PCD_CommunicateWithPICC(uint8_t command,   ///< The command to execute. One of the PCD_Command enums.
                                                    uint8_t waitIRq,    ///< The bits in the ComIrqReg register that signals successful completion of the command.
                                                    uint8_t *sendData,  ///< Pointer to the data to transfer to the FIFO.
                                                    uint8_t sendLen,    ///< Number of bytes to transfer to the FIFO.
                                                    uint8_t *backData,  ///< nullptr or pointer to buffer if data should be read back after executing the command.
                                                    uint8_t *backLen,   ///< In: Max number of bytes to write to *backData. Out: The number of bytes returned.
                                                    uint8_t *validBits, ///< In/Out: The number of valid bits in the last byte. 0 for 8 valid bits.
                                                    uint8_t rxAlign,    ///< In: Defines the bit position in backData[0] for the first bit received. Default 0.
                                                    bool checkCRC )     ///< In: True => The last two bytes of the response is assumed to be a CRC_A that must be validated.
{
    // Prepare values for BitFramingReg
    uint8_t txLastBits = validBits ? *validBits : 0;
    uint8_t bitFraming = (rxAlign << 4) + txLastBits;        // RxAlign = BitFramingReg[6..4]. TxLastBits = BitFramingReg[2..0]

    PCD_WriteRegister(CommandReg, PCD_Idle);            // Stop any active command.
    PCD_WriteRegister(ComIrqReg, 0x7F);                    // Clear all seven interrupt request bits
    PCD_WriteRegister(FIFOLevelReg, 0x80);                // FlushBuffer = 1, FIFO initialization
    PCD_WriteRegister(FIFODataReg, sendLen, sendData);    // Write sendData to the FIFO
    PCD_WriteRegister(BitFramingReg, bitFraming);        // Bit adjustments
    PCD_WriteRegister(CommandReg, command);                // Execute the command
    if (command == PCD_Transceive) {
        PCD_SetRegisterBitMask(BitFramingReg, 0x80);    // StartSend=1, transmission of data starts
    }

    // In PCD_Init() we set the TAuto flag in TModeReg. This means the timer
    // automatically starts when the PCD stops transmitting.
    //
    // Wait here for the command to complete. The bits specified in the
    // `waitIRq` parameter define what bits constitute a completed command.
    // When they are set in the ComIrqReg register, then the command is
    // considered complete. If the command is not indicated as complete in
    // ~36ms, then consider the command as timed out.
#if 0
    const uint32_t deadline = millis() + 36;
#else
    const uint32_t deadline = millis() + 5;
#endif
    bool completed = false;

    do {
        uint8_t n = PCD_ReadRegister(ComIrqReg);    // ComIrqReg[7..0] bits are: Set1 TxIRq RxIRq IdleIRq HiAlertIRq LoAlertIRq ErrIRq TimerIRq
        if (n & waitIRq) {                    // One of the interrupts that signal success has been set.
            completed = true;
            break;
        }
        if (n & 0x01) {                        // Timer interrupt - nothing received in 5ms
            return STATUS_TIMEOUT;
        }
        taskYIELD();
    }
    while (static_cast<uint32_t> (millis()) < deadline);

    // 36ms and nothing happened. Communication with the MFRC522 might be down.
    if (!completed) {
        return STATUS_TIMEOUT;
    }

    // Stop now if any errors except collisions were detected.
    uint8_t errorRegValue = PCD_ReadRegister(ErrorReg); // ErrorReg[7..0] bits are: WrErr TempErr reserved BufferOvfl CollErr CRCErr ParityErr ProtocolErr
    if (errorRegValue & 0x13) {     // BufferOvfl ParityErr ProtocolErr
        return STATUS_ERROR;
    }

    uint8_t _validBits = 0;

    // If the caller wants data back, get it from the MFRC522.
    if (backData && backLen) {
        uint8_t n = PCD_ReadRegister(FIFOLevelReg);    // Number of bytes in the FIFO
        if (n > *backLen) {
            return STATUS_NO_ROOM;
        }
        *backLen = n;                                            // Number of bytes returned
        PCD_ReadRegister(FIFODataReg, n, backData, rxAlign);    // Get received data from FIFO
        _validBits = PCD_ReadRegister(ControlReg) & 0x07;        // RxLastBits[2:0] indicates the number of valid bits in the last received byte. If this value is 000b, the whole byte is valid.
        if (validBits) {
            *validBits = _validBits;
        }
    }

    // Tell about collisions
    if (errorRegValue & 0x08) {        // CollErr
        return STATUS_COLLISION;
    }

    // Perform CRC_A validation if requested.
    if (backData && backLen && checkCRC) {
        // In this case a MIFARE Classic NAK is not OK.
        if (*backLen == 1 && _validBits == 4) {
            return STATUS_MIFARE_NACK;
        }
        // We need at least the CRC_A value and all 8 bits of the last byte must be received.
        if (*backLen < 2 || _validBits != 0) {
            return STATUS_CRC_WRONG;
        }
        // Verify CRC_A - do our own calculation and store the control in controlBuffer.
        uint8_t controlBuffer[2];
        CalcCRC(&backData[0], *backLen - 2, controlBuffer);
        if ((backData[*backLen - 2] != controlBuffer[0]) || (backData[*backLen - 1] != controlBuffer[1])) {
            return STATUS_CRC_WRONG;
        }
    }

    return STATUS_OK;
}

MFRC522::StatusCode MFRC522::PICC_RequestA(uint8_t *bufferATQA, uint8_t *bufferSize)
{
    return PICC_REQA_or_WUPA(PICC_CMD_REQA, bufferATQA, bufferSize);
}

MFRC522::StatusCode MFRC522::PICC_REQA_or_WUPA(uint8_t command, uint8_t *bufferATQA, uint8_t *bufferSize)
{
    uint8_t validBits;
    MFRC522::StatusCode status;

    if (bufferATQA == nullptr || *bufferSize < 2) { // The ATQA response is 2 bytes long.
        return STATUS_NO_ROOM;
    }
    PCD_ClearRegisterBitMask(CollReg, 0x80);        // ValuesAfterColl=1 => Bits received after collision are cleared.
    validBits = 7;                                  // For REQA and WUPA we need the short frame format - transmit only 7 bits of the last (and only) byte. TxLastBits = BitFramingReg[2..0]
    status = PCD_TransceiveData(&command, 1, bufferATQA, bufferSize, &validBits);
    if (status != STATUS_OK) {
        return status;
    }
    if (*bufferSize != 2 || validBits != 0) {       // ATQA must be exactly 16 bits.
        return STATUS_ERROR;
    }
    return STATUS_OK;
}

MFRC522::StatusCode MFRC522::MIFARE_Read(uint8_t blockAddr,     ///< MIFARE Classic: The block (0-0xff) number. MIFARE Ultralight: The first page to return data from.
                                        uint8_t *buffer,        ///< The buffer to store the data in
                                        uint8_t *bufferSize )   ///< Buffer size, at least 18 bytes. Also number of bytes returned if STATUS_OK.
{
    // Sanity check
    if (buffer == nullptr || *bufferSize < 18) {
        return STATUS_NO_ROOM;
    }

    // Build command buffer
    buffer[0] = PICC_CMD_MF_READ;
    buffer[1] = blockAddr;
    // Calculate CRC_A
    CalcCRC(buffer, 2, &buffer[2]);

    // Transmit the buffer and receive the response, validate CRC_A.
    return PCD_TransceiveData(buffer, 4, buffer, bufferSize, nullptr, 0, true);
}
