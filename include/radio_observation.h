#pragma once

#include <Arduino.h>

enum RadioFrameType {
    FRAME_UNKNOWN,
    FRAME_MANAGEMENT,
    FRAME_CONTROL,
    FRAME_DATA
};

struct RadioObservation {
    uint8_t mac[6] = {0, 0, 0, 0, 0, 0};
    int rssi = 0;
    int channel = 0;
    unsigned long timestamp = 0;
    bool hasSSID = false;
    String ssid;
    RadioFrameType frameType = FRAME_UNKNOWN;
    uint8_t frameSubtype = 0;
};

String formatMac(const uint8_t mac[6]);
