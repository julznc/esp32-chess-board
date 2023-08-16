#ifndef __UI_BUTTONS_H__
#define __UI_BUTTONS_H__

#include <atomic>
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
    bool shortPressed() { return (1 == getCount()); }
    uint32_t pressedDuration();
    uint32_t getCount();
    void resetCount();

private:
    int btnPin;
    std::atomic<int>    previousSteadyState;
    std::atomic<int>    lastSteadyState;
    std::atomic<int>    lastFlickerableState;

    std::atomic<uint32_t>   lastDebounceTime;
    std::atomic<uint32_t>   lastPressedTime;
    std::atomic<uint32_t>   pressedCount;
};

bool init(void);
void loop(void);

extern PushButton pb1;
extern PushButton pb2;
extern PushButton pb3;


#define MAIN_BTN        ui::btn::pb1
#define LEFT_BTN        ui::btn::pb2
#define RIGHT_BTN       ui::btn::pb3

} // namespace ui::btn

#endif // __UI_BUTTONS_H__
