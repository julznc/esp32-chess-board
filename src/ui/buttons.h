#ifndef __UI_BUTTONS_H__
#define __UI_BUTTONS_H__

#include <stdint.h>

namespace ui::btn
{

class PushButton
{
public:
    PushButton(int pin);

    void loop();
    bool pressed();
    bool released();
    uint32_t pressedDuration();
    uint32_t getCount();

private:
    int btnPin;
    int previousSteadyState;
    int lastSteadyState;
    int lastFlickerableState;

    uint32_t lastDebounceTime;
    uint32_t lastPressedTime;
    uint32_t pressedCount;
};

bool init(void);
void loop(void);

extern PushButton pb1;
extern PushButton pb2;
extern PushButton pb3;

} // namespace ui::btn

#endif // __UI_BUTTONS_H__
