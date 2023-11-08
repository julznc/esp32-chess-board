
#pragma once

#include "graphics/graphics.h"


// copied from "Adafruit_GrayOLED" library

class OledMono : public Graphics
{
public:
    // i2c write function
    typedef bool (*write_func_t)(const uint8_t *buffer, size_t buffer_len, bool stop,
                                const uint8_t *prefix_buffer, size_t prefix_len);

    OledMono(uint16_t w, uint16_t h, write_func_t fp_write);

    void clearDisplay(void);

    void oled_command(uint8_t c);
    bool oled_commandList(const uint8_t *c, uint8_t n);


protected:
    write_func_t fp_write;
};
