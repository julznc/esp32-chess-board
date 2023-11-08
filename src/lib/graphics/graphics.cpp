
#include "globals.h"

#include "graphics.h"
#include "glcdfont.h"


Graphics::Graphics(int16_t w, int16_t h) : WIDTH(w), HEIGHT(h), _width(w), _height(h)
{
    cursor_y = cursor_x = 0;
    textcolor = textbgcolor = 0xFFFF;
    textsize_x = textsize_y = 1;
    wrap = true;
    _cp437 = false;
    gfxFont = NULL;
}

#ifndef _swap_int16_t
#define _swap_int16_t(a, b)                                                    \
  {                                                                            \
    int16_t t = a;                                                             \
    a = b;                                                                     \
    b = t;                                                                     \
  }
#endif

void Graphics::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)
{
  int16_t steep = abs(y1 - y0) > abs(x1 - x0);
  if (steep) {
    _swap_int16_t(x0, y0);
    _swap_int16_t(x1, y1);
  }

  if (x0 > x1) {
    _swap_int16_t(x0, x1);
    _swap_int16_t(y0, y1);
  }

  int16_t dx, dy;
  dx = x1 - x0;
  dy = abs(y1 - y0);

  int16_t err = dx / 2;
  int16_t ystep;

  if (y0 < y1) {
    ystep = 1;
  } else {
    ystep = -1;
  }

  for (; x0 <= x1; x0++) {
    if (steep) {
      drawPixel(y0, x0, color);
    } else {
      drawPixel(x0, y0, color);
    }
    err -= dy;
    if (err < 0) {
      y0 += ystep;
      err += dx;
    }
  }
}

void Graphics::drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color)
{
    drawLine(x, y, x, y + h - 1, color);
}

void Graphics::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color)
{
    drawLine(x, y, x + w - 1, y, color);
}

void Graphics::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    for (int16_t i = x; i < x + w; i++) {
        drawFastVLine(i, y, h, color);
    }
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

void Graphics::drawChar(int16_t x, int16_t y, unsigned char c, uint16_t color, uint16_t bg, uint8_t size_x, uint8_t size_y)
{
  //LOGD("%s(%d-%d, %c, %04x/%04x, %u/%u)", __func__, x, y, c, color, bg, size_x, size_y);
  if (!gfxFont) { // 'Classic' built-in font

    if ((x >= _width) ||              // Clip right
        (y >= _height) ||             // Clip bottom
        ((x + 6 * size_x - 1) < 0) || // Clip left
        ((y + 8 * size_y - 1) < 0))   // Clip top
      return;

    if (!_cp437 && (c >= 176))
      c++; // Handle 'classic' charset behavior

    for (int8_t i = 0; i < 5; i++) { // Char bitmap = 5 columns
      uint8_t line = font[c * 5 + i];
      for (int8_t j = 0; j < 8; j++, line >>= 1) {
        if (line & 1) {
          if (size_x == 1 && size_y == 1)
            drawPixel(x + i, y + j, color);
          else
            fillRect(x + i * size_x, y + j * size_y, size_x, size_y, color);
        } else if (bg != color) {
          if (size_x == 1 && size_y == 1)
            drawPixel(x + i, y + j, bg);
          else
            fillRect(x + i * size_x, y + j * size_y, size_x, size_y, bg);
        }
      }
    }
    if (bg != color) { // If opaque, draw vertical line for last column
      if (size_x == 1 && size_y == 1)
        drawFastVLine(x + 5, y, 8, bg);
      else
        fillRect(x + 5 * size_x, y, size_x, 8 * size_y, bg);
    }

  } else { // Custom font

    // to do

  } // End classic vs custom font
}

void Graphics::setFont(const GFXfont *f)
{
    if (f) {
        if (!gfxFont) {
            cursor_y += 6;
        }
    } else if (gfxFont) {
        cursor_y -= 6;
    }
    gfxFont = (GFXfont *)f;
}

size_t Graphics::print(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int len = vsnprintf(print_buf, sizeof(print_buf), format, args);
    va_end(args);

    //LOGD("%s(\"%s\")", __func__, print_buf);

    return (len > 0) ? write((const uint8_t *)print_buf, len) : (0);
}

size_t Graphics::write(uint8_t c)
{
  if (!gfxFont) { // 'Classic' built-in font

    if (c == '\n') {              // Newline?
      cursor_x = 0;               // Reset x to zero,
      cursor_y += textsize_y * 8; // advance y one line
    } else if (c != '\r') {       // Ignore carriage returns
      if (wrap && ((cursor_x + textsize_x * 6) > _width)) { // Off right?
        cursor_x = 0;                                       // Reset x to zero,
        cursor_y += textsize_y * 8; // advance y one line
      }
      drawChar(cursor_x, cursor_y, c, textcolor, textbgcolor, textsize_x, textsize_y);
      cursor_x += textsize_x * 6; // Advance x one char
    }

  } else { // Custom font

    // to do
  }
  return 1;
}

size_t Graphics::write(const uint8_t *buffer, size_t size)
{
    size_t n = 0;
    while(size--) {
        n += write(*buffer++);
    }
    return n;
}
