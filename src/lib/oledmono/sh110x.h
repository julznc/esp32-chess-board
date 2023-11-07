
#pragma once

#include "oledmono.h"

// copied from "Adafruit_SH110X" library

class SH110X : public OledMono
{
public:
    SH110X(uint16_t w, uint16_t h);
};


class SH1106G : public SH110X
{
public:
    SH1106G(uint16_t w, uint16_t h);

    bool init(void);
};