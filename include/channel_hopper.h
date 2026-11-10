#pragma once

#include <Arduino.h>

class ChannelHopper {
public:
    ChannelHopper(uint8_t firstChannel = 1, uint8_t lastChannel = 11,
                  unsigned long dwellTimeMs = 250);

    bool begin(unsigned long now);
    bool update(unsigned long now);
    uint8_t currentChannel() const;

private:
    uint8_t firstChannel_;
    uint8_t lastChannel_;
    uint8_t currentChannel_;
    unsigned long dwellTimeMs_;
    unsigned long lastHopMs_;
    bool initialized_;
};
