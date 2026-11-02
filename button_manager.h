#pragma once

#include <stdint.h>
#include <Arduino.h>
#include "config.h"

enum ButtonEvent {
    EVENT_NONE = 0,
    EVENT_NAV_CLICK,    // Black Button single tap (cycle screen / carousel)
    EVENT_SOS_CLICK,    // Red Button single tap (send highlighted message)
    EVENT_PANIC_HOLD    // Red Button held for 3 seconds (INSTANT PANIC SOS)
};

class ButtonManager {
public:
    ButtonManager();
    void begin();
    ButtonEvent update();  // Call periodically in loop or FreeRTOS task

private:
    // Red SOS Button state tracking
    bool     _sosLastState;
    uint32_t _sosPressTime;
    bool     _sosLongPressFired;

    // Black NAV Button state tracking
    bool     _navLastState;
    uint32_t _navPressTime;
};
