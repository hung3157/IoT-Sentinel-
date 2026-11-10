#pragma once

#include <Arduino.h>

#include "distance_estimator.h"
#include "radio_observation.h"
#include "rssi_filter.h"

enum class RssiTrend : uint8_t {
    Approaching,
    Stable,
    MovingAway
};

struct Device {
    uint8_t mac[6] = {0, 0, 0, 0, 0, 0};
    String ssid;
    bool hasSSID = false;
    int rawRSSI = 0;
    float filteredRSSI = 0.0f;
    float distanceMeters = 0.0f;
    bool hasDistanceEstimate = false;
    int channel = 0;
    unsigned long firstSeen = 0;
    unsigned long lastSeen = 0;
    unsigned long seenCount = 0;
    uint32_t packetCount = 0;
    uint32_t managementFrameCount = 0;
    uint32_t controlFrameCount = 0;
    uint32_t dataFrameCount = 0;
    uint16_t activityWindowCount = 0;
    uint16_t packetsPerMinute = 0;
    unsigned long activityWindowStarted = 0;
    RssiTrend rssiTrend = RssiTrend::Stable;
    bool known = false;
};

class DeviceManager {
public:
    static const size_t kMaxDevices = 32;

    explicit DeviceManager(unsigned long timeoutMs = 300000);

    bool update(const RadioObservation& observation);
    void expire(unsigned long now);
    bool setKnown(const uint8_t mac[6], bool known);

    size_t deviceCount() const;
    const Device* deviceAt(size_t index) const;
    const Device* findByMac(const uint8_t mac[6]) const;
    bool configureDistance(float rssiAtOneMeter, float pathLossExponent);
    unsigned long timeoutMs() const;

private:
    static bool isTrackableMac(const uint8_t mac[6]);
    int findIndex(const uint8_t mac[6]) const;
    int findKnownIndex(const uint8_t mac[6]) const;
    int findOldestIndex(unsigned long now) const;
    void removeAt(size_t index);
    bool isKnown(const uint8_t mac[6]) const;
    bool rememberKnown(const uint8_t mac[6]);

    Device devices_[kMaxDevices];
    RssiFilter rssiFilters_[kMaxDevices];
    uint8_t knownMacs_[kMaxDevices][6];
    size_t deviceCount_;
    size_t knownCount_;
    unsigned long timeoutMs_;
    DistanceEstimator distanceEstimator_;
};
