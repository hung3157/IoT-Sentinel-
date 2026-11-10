#include "button_input.h"

ButtonInput::ButtonInput(uint8_t nextPin, uint8_t modePin,
                         unsigned long debounceMs)
    : next_{nextPin, HIGH, HIGH, 0},
      mode_{modePin, HIGH, HIGH, 0},
      debounceMs_(debounceMs) {}

void ButtonInput::begin() {
    pinMode(next_.pin, INPUT_PULLUP);
    pinMode(mode_.pin, INPUT_PULLUP);
    next_.stableState = digitalRead(next_.pin);
    next_.lastReading = next_.stableState;
    mode_.stableState = digitalRead(mode_.pin);
    mode_.lastReading = mode_.stableState;
}

ButtonEvents ButtonInput::update(unsigned long now) {
    ButtonEvents events{};
    events.nextPressed = updateButton(next_, now);
    events.modePressed = updateButton(mode_, now);
    return events;
}

bool ButtonInput::updateButton(ButtonState& button, unsigned long now) {
    const bool reading = digitalRead(button.pin);
    if (reading != button.lastReading) {
        button.lastReading = reading;
        button.changedAt = now;
    }

    if (reading != button.stableState &&
        (now - button.changedAt) >= debounceMs_) {
        button.stableState = reading;
        return button.stableState == LOW;
    }
    return false;
}
