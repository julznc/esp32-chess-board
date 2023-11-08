
#include "globals.h"

#include "sh110x.h"


SH110X::SH110X(uint16_t w, uint16_t h, write_func_t fp_write) : OledMono(w, h, fp_write)
{
    //
}



SH1106G::SH1106G(uint16_t w, uint16_t h, write_func_t fp_write) : SH110X(w, h, fp_write)
{
    //
}

bool SH1106G::init(void)
{
    static const uint8_t _init[] = {
        SH110X_DISPLAYOFF,               // 0xAE
        SH110X_SETDISPLAYCLOCKDIV, 0x80, // 0xD5, 0x80,
        SH110X_SETMULTIPLEX, 0x3F,       // 0xA8, 0x3F,
        SH110X_SETDISPLAYOFFSET, 0x00,   // 0xD3, 0x00,
        SH110X_SETSTARTLINE,             // 0x40
        SH110X_DCDC, 0x8B,               // DC/DC on
        SH110X_SEGREMAP + 1,             // 0xA1
        SH110X_COMSCANDEC,               // 0xC8
        SH110X_SETCOMPINS, 0x12,         // 0xDA, 0x12,
        SH110X_SETCONTRAST, 0xFF,        // 0x81, 0xFF
        SH110X_SETPRECHARGE, 0x1F,       // 0xD9, 0x1F,
        SH110X_SETVCOMDETECT, 0x40,      // 0xDB, 0x40,
        0x33,                            // Set VPP to 9V
        SH110X_NORMALDISPLAY,
        SH110X_MEMORYMODE, 0x10,         // 0x20, 0x00
        SH110X_DISPLAYALLON_RESUME,
    };

    if (!oled_commandList(_init, sizeof(_init))) {
        return false;
    }

    delayms(100);                     // 100ms delay recommended
    oled_command(SH110X_DISPLAYON); // 0xaf

    clearDisplay();

    return true; // Success
}
