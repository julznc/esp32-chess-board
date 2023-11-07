
#pragma once

#include "graphics/graphics.h"


// copied from "Adafruit_GrayOLED" library

class OledMono : public Graphics
{
public:
    OledMono(uint16_t w, uint16_t h);
};
