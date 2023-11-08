
#include "globals.h"

#include "graphics.h"


Graphics::Graphics(int16_t w, int16_t h) : WIDTH(w), HEIGHT(h), _width(w), _height(h)
{
    //
}

void Graphics::drawBitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint16_t color)
{
    int16_t byteWidth = (w + 7) / 8; // Bitmap scanline pad = whole byte
    uint8_t b = 0;

    for (int16_t j = 0; j < h; j++, y++)
    {
        for (int16_t i = 0; i < w; i++)
        {
            if (i & 7)  b <<= 1;
            else        b = bitmap[j * byteWidth + i / 8];

            if (b & 0x80)
                drawPixel(x + i, y, color);
        }
    }
}
