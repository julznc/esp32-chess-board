
#pragma once


// copied from "Adafruit_GFX" library
#include "gfxfont.h"

class Graphics
{
public:
    Graphics(int16_t w, int16_t h);

    virtual void drawPixel(int16_t x, int16_t y, uint16_t color) = 0;

    virtual void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
    virtual void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color);
    virtual void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color);
    virtual void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);

    void drawBitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint16_t color);
    void drawChar(int16_t x, int16_t y, unsigned char c, uint16_t color, uint16_t bg, uint8_t size_x, uint8_t size_y);

    void setFont(const GFXfont *f);
    void setCursor(int16_t x, int16_t y) { cursor_x = x; cursor_y = y;}
    void setTextColor(uint16_t c, uint16_t bg) { textcolor = c; textbgcolor = bg; }
    void cp437(bool x = true) { _cp437 = x; }

    size_t print(const char *format, ...);
    virtual size_t write(uint8_t);
    virtual size_t write(const uint8_t *buffer, size_t size);

protected:
    int16_t WIDTH;        ///< This is the 'raw' display width - never changes
    int16_t HEIGHT;       ///< This is the 'raw' display height - never changes
    int16_t _width;       ///< Display width as modified by current rotation
    int16_t _height;      ///< Display height as modified by current rotation
    int16_t cursor_x;     ///< x location to start print()ing text
    int16_t cursor_y;     ///< y location to start print()ing text
    uint16_t textcolor;   ///< 16-bit background color for print()
    uint16_t textbgcolor; ///< 16-bit text color for print()
    uint8_t textsize_x;   ///< Desired magnification in X-axis of text to print()
    uint8_t textsize_y;   ///< Desired magnification in Y-axis of text to print()
    bool wrap;            ///< If set, 'wrap' text at right edge of display
    bool _cp437;          ///< If set, use correct CP437 charset (default is off)
    GFXfont *gfxFont;     ///< Pointer to special font
    char print_buf[128];
};
