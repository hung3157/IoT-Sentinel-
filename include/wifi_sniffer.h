#pragma once

#include <Arduino.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>

#include "radio_observation.h"

class WiFiSniffer {
public:
    static const size_t kQueueCapacity = 64;

    WiFiSniffer();

    bool begin();
    void stop();
    bool popObservation(RadioObservation& observation);
    uint32_t droppedCount() const;

private:
    struct PendingObservation {
        uint8_t mac[6];
        int8_t rssi;
        uint8_t channel;
        unsigned long timestamp;
        RadioFrameType frameType;
        uint8_t frameSubtype;
    };

    static void receiveCallback(void* buffer,
                                wifi_promiscuous_pkt_type_t packetType);
    void handlePacket(const wifi_promiscuous_pkt_t& packet,
                      wifi_promiscuous_pkt_type_t packetType);
    bool enqueue(const PendingObservation& observation);

    static WiFiSniffer* activeInstance_;
    PendingObservation queue_[kQueueCapacity];
    size_t queueHead_;
    size_t queueTail_;
    size_t queueCount_;
    uint32_t droppedCount_;
    mutable portMUX_TYPE queueMux_;
    bool initialized_;
};
