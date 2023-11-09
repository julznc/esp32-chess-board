
#pragma once

#include <atomic>
#include <stdint.h>

namespace ui::btn
{

class PushButton
{
public:
    PushButton(gpio_num_t pin);

    void loop();
    bool pressed();
    bool released();
    bool shortPressed() { return (1 == getCount()); }
    uint32_t pressedDuration();
    uint32_t getCount();
    void resetCount();

private:
    gpio_num_t btnPin;
    std::atomic<int>    previousSteadyState;
    std::atomic<int>    lastSteadyState;
    std::atomic<int>    lastFlickerableState;

    std::atomic<uint32_t>   lastDebounceTime;
    std::atomic<uint32_t>   lastPressedTime;
    std::atomic<uint32_t>   pressedCount;
};

void loop(void);

extern PushButton pb1;
extern PushButton pb2;
extern PushButton pb3;


#define MAIN_BTN        ui::btn::pb1
#define LEFT_BTN        ui::btn::pb2
#define RIGHT_BTN       ui::btn::pb3

} // namespace ui::btn