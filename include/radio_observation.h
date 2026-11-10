#pragma once

#include <Arduino.h>

enum class RadioFrameType : uint8_t {
    Unknown,
    Management,
    Control,
    Data
};

struct RadioObservation {

    uint8_t mac[6] = {0, 0, 0, 0, 0, 0};
    int rssi = 0; //Received Signal Strength Indication
    int channel = 0;
    unsigned long timestamp = 0;
    bool hasSSID = false; // Indicates if the observation has a valid SSID
    String ssid = "";
    RadioFrameType frameType = RadioFrameType::Unknown;
    uint8_t frameSubtype = 0;
    
};
