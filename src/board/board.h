#ifndef __BOARD_H__
#define __BOARD_H__


namespace brd
{

/*
  NTAG213 RFID stickers
  note: use "NFC Tools" app to read/write data* to the rfid sticker
    *where data = single-character text record representing the piece. e.g. 'k' for "black king"
  */
#define NTAG_DATA_START_PAGE        (4)  // 16 bytes per page
// sample tag data (TLVs format) : 01 03 a0 0c 34 03 08 d1 01 04 54 02 65 6e 6b fe
// LOCK control (tag 0x01 len 3) : 01 03 a0 0c 34
// NDEF message (tag 0x03 len 8) : 03 08 d1 01 04 54 02 65 6e 6b
// terminator   (tag 0xfe len 0) : fe
#define NTAG_DATA_PIECE_OFFSET      (14)  // e.g. the 0x6b ('k') value



void init(void);
const uint8_t *pu8_pieces(void);
const uint32_t *pu32_toggle_ms(void);

} // namespace brd

#endif // __BOARD_H__
