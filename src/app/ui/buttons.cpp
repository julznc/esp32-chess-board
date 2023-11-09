
#include "globals.h"
#include "buttons.h"


namespace ui::btn
{

PushButton pb1(PIN_BTN_1);
PushButton pb2(PIN_BTN_2);
PushButton pb3(PIN_BTN_3);


// copied from https://github.com/ArduinoGetStarted/button
PushButton::PushButton(gpio_num_t pin) : btnPin(pin),
    previousSteadyState(1), lastSteadyState(1), lastFlickerableState(1),
    lastDebounceTime(0), lastPressedTime(0), pressedCount(0)
{
    //pinMode(pin); // done on gpio_init
}

void PushButton::loop()
{
    int currentState = gpio_get_level(btnPin);
    unsigned long currentTime = millis();

    if (currentState != lastFlickerableState) {
        lastDebounceTime = currentTime;
        lastFlickerableState = currentState;
    }

    if ((currentTime - lastDebounceTime) > 10 /*debounceTime*/) {
        previousSteadyState.store(lastSteadyState.load());
        lastSteadyState = currentState;
    }

    if (currentTime - lastDebounceTime > 1000UL) {
        pressedCount = 0; // auto-reset counter after
    }

    if (previousSteadyState != lastSteadyState) {
        if (pressed()) {
            //LOGD("pressed %d", btnPin);
            lastPressedTime = currentTime;
        }
        else if (released()) {
            //LOGD("released %d", btnPin);
            if (currentTime - lastPressedTime < 1000UL) {
                pressedCount++; // count short press
            }
        }
    }
}

bool PushButton::pressed()
{
    return ((previousSteadyState == 1) && (lastSteadyState == 0));
}

bool PushButton::released()
{
    return ((previousSteadyState == 0) && (lastSteadyState == 1));
}

uint32_t PushButton::pressedDuration()
{
    return ((previousSteadyState == 0) && (lastSteadyState == 0)) ? (millis() - lastPressedTime) : (0);
}

uint32_t PushButton::getCount()
{
    return pressedCount;
}

void PushButton::resetCount()
{
    pressedCount = 0;
    if (lastSteadyState == 0) {
        lastPressedTime = millis();
    }
}

void loop(void)
{
    pb1.loop();
    pb2.loop();
    pb3.loop();
}

} // namespace ui::btn
