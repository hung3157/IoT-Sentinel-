#include "channel_hopper.h"

#include <esp_wifi.h>

ChannelHopper::ChannelHopper(uint8_t firstChannel, uint8_t lastChannel,
                             unsigned long dwellTimeMs)
    : firstChannel_(firstChannel),
      lastChannel_(lastChannel),
      currentChannel_(firstChannel),
      dwellTimeMs_(dwellTimeMs),
      lastHopMs_(0),
      initialized_(false) {}

bool ChannelHopper::begin(unsigned long now) {
    if (firstChannel_ < 1 || lastChannel_ > 13 ||
        firstChannel_ > lastChannel_ || dwellTimeMs_ == 0) {
        Serial.println("Invalid channel hopper configuration");
        return false;
    }

    currentChannel_ = firstChannel_;
    const esp_err_t result = esp_wifi_set_channel(
        currentChannel_, WIFI_SECOND_CHAN_NONE);
    if (result != ESP_OK) {
        Serial.printf("Unable to set WiFi channel %u (%d)\n",
                      currentChannel_, result);
        return false;
    }

    lastHopMs_ = now;
    initialized_ = true;
    return true;
}

bool ChannelHopper::update(unsigned long now) {
    if (!initialized_ || (now - lastHopMs_) < dwellTimeMs_) {
        return false;
    }

    const uint8_t nextChannel =
        currentChannel_ >= lastChannel_ ? firstChannel_ : currentChannel_ + 1;
    const esp_err_t result =
        esp_wifi_set_channel(nextChannel, WIFI_SECOND_CHAN_NONE);
    if (result != ESP_OK) {
        Serial.printf("Unable to set WiFi channel %u (%d)\n",
                      nextChannel, result);
        lastHopMs_ = now;
        return false;
    }

    currentChannel_ = nextChannel;
    lastHopMs_ = now;
    return true;
}

uint8_t ChannelHopper::currentChannel() const {
    return currentChannel_;
}
