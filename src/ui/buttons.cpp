
#include "globals.h"
#include "buttons.h"


namespace ui::btn
{

PushButton pb1(BTN_1_PIN);
PushButton pb2(BTN_2_PIN);
PushButton pb3(BTN_3_PIN);


// copied from https://github.com/ArduinoGetStarted/button
PushButton::PushButton(int pin) : btnPin(pin),
    previousSteadyState(HIGH), lastSteadyState(HIGH), lastFlickerableState(HIGH),
    lastDebounceTime(0), lastPressedTime(0), pressedCount(0)
{
    //pinMode(pin); // done on init
}

void PushButton::loop()
{
    int currentState = digitalRead(btnPin);
    unsigned long currentTime = millis();

    if (currentState != lastFlickerableState) {
        lastDebounceTime = currentTime;
        lastFlickerableState = currentState;
    }

    if ((currentTime - lastDebounceTime) > 10 /*debounceTime*/) {
        previousSteadyState = lastSteadyState;
        lastSteadyState = currentState;
    }

    if (currentTime - lastDebounceTime > 1000UL) {
        pressedCount = 0; // auto-reset counter after
    }

    if (previousSteadyState != lastSteadyState) {
        if (pressed()) {
            lastPressedTime = currentTime;
        }
        else if (released()) {
            if (currentTime - lastPressedTime < 1000UL) {
                pressedCount++; // count short press
            }
        }
    }
}

bool PushButton::pressed()
{
    return ((previousSteadyState == HIGH) && (lastSteadyState == LOW));
}

bool PushButton::released()
{
    return ((previousSteadyState == LOW) && (lastSteadyState == HIGH));
}

uint32_t PushButton::pressedDuration()
{
    return ((previousSteadyState == LOW) && (lastSteadyState == LOW)) ? (millis() - lastPressedTime) : (0);
}

uint32_t PushButton::getCount()
{
    return pressedCount;
}


bool init(void)
{
    pinMode(BTN_1_PIN, INPUT_PULLUP);
    pinMode(BTN_2_PIN, INPUT_PULLUP);
    pinMode(BTN_3_PIN, INPUT_PULLUP);
    return true;
}

void loop(void)
{
    pb1.loop();
    pb2.loop();
    pb3.loop();
}

} // namespace ui::btn
