#pragma once

#include <Arduino.h>
#include <WiFi.h>

#include "radio_observation.h"

class WiFiScanner {
    public:

    WiFiScanner(unsigned long timeDiff);

    void begin(); 
    bool scanOnce();
    bool update();

    const RadioObservation* observations() const;
    size_t observationCount() const;
    
    static String formatMac(const uint8_t mac[6]);
    static void printObservation(const RadioObservation& observation);
        
    private:

    static const size_t kMaxObservations = 5;

    unsigned long scanIntervalMs_;
    unsigned long lastScanMs_;
    bool hasScanned_;
    bool scanInProgress_;
        
    size_t observationCount_;
    RadioObservation observations_[kMaxObservations];

    bool collectResults(int foundNetworks);
};
