
#include "globals.h"

#include "sh110x.h"


SH110X::SH110X(uint16_t w, uint16_t h)
    : OledMono(w, h)
{
    //
}



SH1106G::SH1106G(uint16_t w, uint16_t h)
    : SH110X(w, h)
{
    //
}

bool SH1106G::init(void)
{
    return false;
}
