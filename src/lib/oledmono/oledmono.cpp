
#include <string>
#include "globals.h"

#include "oledmono.h"

using namespace::std;


OledMono::OledMono(uint16_t w, uint16_t h, write_func_t fp_wrt) : Graphics(w, h), fp_write(fp_wrt)
{
    framebuff_sz = 1 /*bpp=1*/ * WIDTH * ((HEIGHT + 7) / 8);
    framebuff = (uint8_t *)malloc(framebuff_sz);
}

void OledMono::drawPixel(int16_t x, int16_t y, uint16_t color)
{
    if ((x >= 0) && (x < _width) && (y >= 0) && (y < _height))
    {
        // adjust dirty window
        window_x1 = min(window_x1, x);
        window_y1 = min(window_y1, y);
        window_x2 = max(window_x2, x);
        window_y2 = max(window_y2, y);

        switch (color)
        {
        case MONOOLED_WHITE:
            framebuff[x + (y / 8) * WIDTH] |= (1 << (y & 7));
            break;
        case MONOOLED_BLACK:
            framebuff[x + (y / 8) * WIDTH] &= ~(1 << (y & 7));
            break;
        case MONOOLED_INVERSE:
            framebuff[x + (y / 8) * WIDTH] ^= (1 << (y & 7));
            break;
        }
    }
}

void OledMono::clearDisplay(void)
{
    assert(NULL != framebuff);
    memset(framebuff, 0, framebuff_sz);

    window_x1 = 0;
    window_y1 = 0;
    window_x2 = WIDTH - 1;
    window_y2 = HEIGHT - 1;
}

void OledMono::oled_command(uint8_t c)
{
    uint8_t buf[2] = {0x00, c}; // Co = 0, D/C = 0
    (void)fp_write(buf, 2, true, NULL, 0);
}

bool OledMono::oled_commandList(const uint8_t *c, uint8_t n)
{
    uint8_t dc_byte = 0x00; // Co = 0, D/C = 0

    return fp_write(c, n, true, &dc_byte, 1);
}
