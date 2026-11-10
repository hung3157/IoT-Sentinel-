#pragma once

#include <Arduino.h>

struct ButtonEvents {
    bool nextPressed;
    bool modePressed;
};

class ButtonInput {
public:
    ButtonInput(uint8_t nextPin, uint8_t modePin,
                unsigned long debounceMs = 35);

    void begin();
    ButtonEvents update(unsigned long now);

private:
    struct ButtonState {
        uint8_t pin;
        bool stableState;
        bool lastReading;
        unsigned long changedAt;
    };

    bool updateButton(ButtonState& button, unsigned long now);

    ButtonState next_;
    ButtonState mode_;
    unsigned long debounceMs_;
};
