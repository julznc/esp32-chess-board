
#include <string>
#include "globals.h"

#include "sh110x.h"
#include "splash.h"

using namespace::std;


SH110X::SH110X(uint16_t w, uint16_t h, write_func_t fp_write) : OledMono(w, h, fp_write), _page_start_offset(0)
{
    //
}

void SH110X::display(void)
{
    uint8_t *ptr = framebuff;
    uint8_t dc_byte = 0x40;
    uint8_t pages = ((HEIGHT + 7) / 8);
    uint8_t bytes_per_page = WIDTH;

    uint8_t first_page = window_y1 / 8;
    //  uint8_t last_page = (window_y2 + 7) / 8;
    uint8_t page_start = min(bytes_per_page, (uint8_t)window_x1);
    uint8_t page_end = (uint8_t)max((int)0, (int)window_x2);

    taskYIELD();

    for (uint8_t p = first_page; p < pages; p++)
    {
        uint8_t bytes_remaining = bytes_per_page;
        ptr = framebuff + (uint16_t)p * (uint16_t)bytes_per_page;
        ptr += page_start;
        bytes_remaining -= page_start;
        bytes_remaining -= (WIDTH - 1) - page_end;

        uint16_t maxbuff = 127; // i2c_dev->maxBufferSize() - 1;

        uint8_t cmd[] = {
          0x00, (uint8_t)(SH110X_SETPAGEADDR + p),
          (uint8_t)(0x10 + ((page_start + _page_start_offset) >> 4)),
          (uint8_t)((page_start + _page_start_offset) & 0xF)};

        fp_write(cmd, 4, true, NULL, 0);

        while (bytes_remaining) {
            uint8_t to_write = min(bytes_remaining, (uint8_t)maxbuff);
            fp_write(ptr, to_write, true, &dc_byte, 1);
            ptr += to_write;
            bytes_remaining -= to_write;
            taskYIELD();
        }
    }

    // reset dirty window
    window_x1 = 1024;
    window_y1 = 1024;
    window_x2 = -1;
    window_y2 = -1;
}


SH1106G::SH1106G(uint16_t w, uint16_t h, write_func_t fp_write) : SH110X(w, h, fp_write)
{
    _page_start_offset = 2; // small offset
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

void SH1106G::splash(void)
{
    drawBitmap((WIDTH - splash1_width) / 2, (HEIGHT - splash1_height) / 2,
                splash1_data, splash1_width, splash1_height, 1);
}
