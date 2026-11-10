#include "wifi_sniffer.h"

#include <WiFi.h>
#include <string.h>

WiFiSniffer* WiFiSniffer::activeInstance_ = nullptr;

WiFiSniffer::WiFiSniffer()
    : queueHead_(0),
      queueTail_(0),
      queueCount_(0),
      droppedCount_(0),
      queueMux_(portMUX_INITIALIZER_UNLOCKED),
      initialized_(false) {}

bool WiFiSniffer::begin() {
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);

    wifi_promiscuous_filter_t filter{};
    filter.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT |
                         WIFI_PROMIS_FILTER_MASK_CTRL |
                         WIFI_PROMIS_FILTER_MASK_DATA;
    esp_err_t result = esp_wifi_set_promiscuous_filter(&filter);
    if (result != ESP_OK) {
        Serial.printf("Unable to set WiFi packet filter (%d)\n", result);
        return false;
    }

    activeInstance_ = this;
    result = esp_wifi_set_promiscuous_rx_cb(receiveCallback);
    if (result != ESP_OK) {
        activeInstance_ = nullptr;
        Serial.printf("Unable to set WiFi receive callback (%d)\n", result);
        return false;
    }

    result = esp_wifi_set_promiscuous(true);
    if (result != ESP_OK) {
        esp_wifi_set_promiscuous_rx_cb(nullptr);
        activeInstance_ = nullptr;
        Serial.printf("Unable to enable promiscuous mode (%d)\n", result);
        return false;
    }

    initialized_ = true;
    return true;
}

void WiFiSniffer::stop() {
    if (!initialized_) {
        return;
    }

    const esp_err_t result = esp_wifi_set_promiscuous(false);
    if (result != ESP_OK) {
        Serial.printf("Unable to disable promiscuous mode (%d)\n", result);
    }
    esp_wifi_set_promiscuous_rx_cb(nullptr);
    if (activeInstance_ == this) {
        activeInstance_ = nullptr;
    }
    initialized_ = false;
}

bool WiFiSniffer::popObservation(RadioObservation& observation) {
    PendingObservation pending{};
    portENTER_CRITICAL(&queueMux_);
    if (queueCount_ == 0) {
        portEXIT_CRITICAL(&queueMux_);
        return false;
    }
    pending = queue_[queueHead_];
    queueHead_ = (queueHead_ + 1) % kQueueCapacity;
    --queueCount_;
    portEXIT_CRITICAL(&queueMux_);

    memcpy(observation.mac, pending.mac, sizeof(observation.mac));
    observation.rssi = pending.rssi;
    observation.channel = pending.channel;
    observation.timestamp = pending.timestamp;
    observation.hasSSID = false;
    observation.ssid = "";
    observation.frameType = pending.frameType;
    observation.frameSubtype = pending.frameSubtype;
    return true;
}

uint32_t WiFiSniffer::droppedCount() const {
    portENTER_CRITICAL(&queueMux_);
    const uint32_t count = droppedCount_;
    portEXIT_CRITICAL(&queueMux_);
    return count;
}

void WiFiSniffer::receiveCallback(
    void* buffer, wifi_promiscuous_pkt_type_t packetType) {
    WiFiSniffer* instance = activeInstance_;
    if (instance == nullptr || buffer == nullptr ||
        packetType == WIFI_PKT_MISC) {
        return;
    }
    instance->handlePacket(
        *static_cast<const wifi_promiscuous_pkt_t*>(buffer), packetType);
}

void WiFiSniffer::handlePacket(
    const wifi_promiscuous_pkt_t& packet,
    wifi_promiscuous_pkt_type_t packetType) {
    const uint8_t* frame = packet.payload;
    const size_t length = packet.rx_ctrl.sig_len;
    if (frame == nullptr || length < 2) {
        return;
    }

    const uint16_t frameControl =
        static_cast<uint16_t>(frame[0]) |
        (static_cast<uint16_t>(frame[1]) << 8);
    const uint8_t frameType = (frameControl >> 2) & 0x03;
    const uint8_t subtype = (frameControl >> 4) & 0x0F;

    PendingObservation pending{};
    pending.frameSubtype = subtype;
    pending.timestamp = millis();
    pending.rssi = packet.rx_ctrl.rssi;
    pending.channel = packet.rx_ctrl.channel;

    if (packetType == WIFI_PKT_MGMT && frameType == 0) {
        if (length < 16) {
            return;
        }
        pending.frameType = RadioFrameType::Management;
        memcpy(pending.mac, frame + 10, sizeof(pending.mac));
    } else if (packetType == WIFI_PKT_DATA && frameType == 2) {
        if (length < 16) {
            return;
        }
        pending.frameType = RadioFrameType::Data;
        memcpy(pending.mac, frame + 10, sizeof(pending.mac));
    } else if (packetType == WIFI_PKT_CTRL && frameType == 1) {
        if (subtype < 8 || subtype == 12 || subtype == 13 ||
            length < 16) {
            return;
        }
        pending.frameType = RadioFrameType::Control;
        memcpy(pending.mac, frame + 10, sizeof(pending.mac));
    } else {
        return;
    }

    if ((pending.mac[0] & 0x01) != 0) {
        return;
    }
    enqueue(pending);
}

bool WiFiSniffer::enqueue(const PendingObservation& observation) {
    portENTER_CRITICAL(&queueMux_);
    if (queueCount_ == kQueueCapacity) {
        ++droppedCount_;
        portEXIT_CRITICAL(&queueMux_);
        return false;
    }

    queue_[queueTail_] = observation;
    queueTail_ = (queueTail_ + 1) % kQueueCapacity;
    ++queueCount_;
    portEXIT_CRITICAL(&queueMux_);
    return true;
}
