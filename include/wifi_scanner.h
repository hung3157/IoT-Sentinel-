#pragma once

#include <Arduino.h>
#include <WiFi.h>

#include "radio_observation.h"

class WiFiScanner {
public:
    static const size_t kMaxObservations = 32;

    explicit WiFiScanner(unsigned long scanIntervalMs);

    bool begin();
    bool update();

    const RadioObservation* observations() const;
    size_t observationCount() const;

    static void printObservation(const RadioObservation& observation);

private:
    unsigned long scanIntervalMs_;
    unsigned long lastScanMs_;
    bool hasScanned_;
    bool scanInProgress_;
    size_t observationCount_;
    RadioObservation observations_[kMaxObservations];

    bool collectResults(int foundNetworks);
};
