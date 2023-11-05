
#include "globals.h"
#include "board.h"


namespace brd
{

bool init()
{
    LOGD("%s()", __func__);
    return true;
}

void loop()
{
    LED_ON();
    delayms(500);
    LED_OFF();
    delayms(500);
}

} // namespace brd
