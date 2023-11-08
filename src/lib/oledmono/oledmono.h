
#pragma once

#include "graphics/graphics.h"


// copied from "Adafruit_GrayOLED" library

#define MONOOLED_BLACK          0   ///< Default black 'color' for monochrome OLEDS
#define MONOOLED_WHITE          1   ///< Default white 'color' for monochrome OLEDS
#define MONOOLED_INVERSE        2   ///< Default inversion command for monochrome OLEDS

class OledMono : public Graphics
{
public:
    // i2c write function
    typedef bool (*write_func_t)(const uint8_t *buffer, size_t buffer_len, bool stop,
                                const uint8_t *prefix_buffer, size_t prefix_len);

    OledMono(uint16_t w, uint16_t h, write_func_t fp_write);

    void drawPixel(int16_t x, int16_t y, uint16_t color);

    virtual void display(void) = 0;
    void clearDisplay(void);

    void oled_command(uint8_t c);
    bool oled_commandList(const uint8_t *c, uint8_t n);


protected:
    write_func_t fp_write;
    uint8_t *framebuff;    ///< Internal 1:1 framebuffer of display mem
    size_t framebuff_sz;
    int16_t window_x1, window_y1, window_x2, window_y2; ///< Dirty tracking window
};
