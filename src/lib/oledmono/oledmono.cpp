
#include "globals.h"

#include "oledmono.h"

OledMono::OledMono(uint16_t w, uint16_t h, write_func_t fp_write) : Graphics(w, h), fp_write(fp_write)
{
    //
}

void OledMono::clearDisplay(void)
{
    //
}

void OledMono::oled_command(uint8_t c)
{
    //
}

bool OledMono::oled_commandList(const uint8_t *c, uint8_t n)
{
    uint8_t dc_byte = 0x00; // Co = 0, D/C = 0

    return fp_write(c, n, true, &dc_byte, 1);
}
