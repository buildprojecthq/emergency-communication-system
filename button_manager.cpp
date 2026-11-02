#include "button_manager.h"

ButtonManager::ButtonManager()
    : _sosLastState(HIGH),
      _sosPressTime(0),
      _sosLongPressFired(false),
      _navLastState(HIGH),
      _navPressTime(0) {}

void ButtonManager::begin() {
    pinMode(BTN_SOS_PIN, INPUT_PULLUP);
    pinMode(BTN_NAV_PIN, INPUT_PULLUP);
}

ButtonEvent ButtonManager::update() {
    uint32_t now = millis();
    ButtonEvent event = EVENT_NONE;

    // Read current raw pin values (LOW = Pressed, HIGH = Released due to INPUT_PULLUP)
    bool sosState = digitalRead(BTN_SOS_PIN);
    bool navState = digitalRead(BTN_NAV_PIN);

    // =========================================================================
    // RED SOS BUTTON HANDLING (Single Click vs 3-Second Hold)
    // =========================================================================
    if (sosState == LOW && _sosLastState == HIGH) {
        // Just pressed down
        _sosPressTime = now;
        _sosLongPressFired = false;
    } else if (sosState == LOW && _sosLastState == LOW) {
        // Being held down
        if (!_sosLongPressFired && (now - _sosPressTime >= BTN_LONG_PRESS_MS)) {
            _sosLongPressFired = true;
            event = EVENT_PANIC_HOLD; // Instant emergency trigger!
        }
    } else if (sosState == HIGH && _sosLastState == LOW) {
        // Just released
        if (!_sosLongPressFired && (now - _sosPressTime >= BTN_DEBOUNCE_MS)) {
            event = EVENT_SOS_CLICK; // Normal click to send selected message
        }
    }
    _sosLastState = sosState;

    // =========================================================================
    // BLACK NAV BUTTON HANDLING (Single Click)
    // =========================================================================
    if (navState == LOW && _navLastState == HIGH) {
        // Just pressed down
        _navPressTime = now;
    } else if (navState == HIGH && _navLastState == LOW) {
        // Just released
        if (now - _navPressTime >= BTN_DEBOUNCE_MS) {
            // Prioritize SOS event if both occurred simultaneously
            if (event == EVENT_NONE) {
                event = EVENT_NAV_CLICK;
            }
        }
    }
    _navLastState = navState;

    return event;
}
