#include "device_manager.h"

#include <string.h>

DeviceManager::DeviceManager(unsigned long timeoutMs)
    : deviceCount_(0), knownCount_(0), timeoutMs_(timeoutMs) {
    for (size_t i = 0; i < kMaxDevices; ++i) {
        devices_[i] = Device{};
        rssiFilters_[i].reset();
        memset(knownMacs_[i], 0, sizeof(knownMacs_[i]));
    }
}

bool DeviceManager::update(const RadioObservation& observation) {
    if (!isTrackableMac(observation.mac)) {
        return false;
    }

    expire(observation.timestamp);
    int index = findIndex(observation.mac);
    if (index < 0) {
        if (deviceCount_ == kMaxDevices) {
            index = findOldestIndex(observation.timestamp);
            removeAt(static_cast<size_t>(index));
        }
        index = static_cast<int>(deviceCount_++);
        devices_[index] = Device{};
        memcpy(devices_[index].mac, observation.mac,
               sizeof(devices_[index].mac));
        devices_[index].firstSeen = observation.timestamp;
        devices_[index].known = isKnown(observation.mac);
        rssiFilters_[index].reset();
    }

    Device& device = devices_[index];
    device.rawRSSI = observation.rssi;
    device.filteredRSSI = rssiFilters_[index].addSample(
        static_cast<float>(observation.rssi));
    device.channel = observation.channel;
    device.lastSeen = observation.timestamp;
    ++device.seenCount;
    const float slope = rssiFilters_[index].trendSlope();
    device.rssiTrend = slope >= 0.75f
                           ? RssiTrend::Approaching
                           : (slope <= -0.75f ? RssiTrend::MovingAway
                                             : RssiTrend::Stable);

    if (observation.frameType != RadioFrameType::Unknown) {
        ++device.packetCount;
        switch (observation.frameType) {
            case RadioFrameType::Management:
                ++device.managementFrameCount;
                break;
            case RadioFrameType::Control:
                ++device.controlFrameCount;
                break;
            case RadioFrameType::Data:
                ++device.dataFrameCount;
                break;
            case RadioFrameType::Unknown:
                break;
        }

        if (device.activityWindowCount == 0 ||
            (observation.timestamp - device.activityWindowStarted) >=
                60000UL) {
            device.activityWindowStarted = observation.timestamp;
            device.activityWindowCount = 0;
        }
        if (device.activityWindowCount < UINT16_MAX) {
            ++device.activityWindowCount;
        }
        const unsigned long elapsed =
            observation.timestamp - device.activityWindowStarted;
        if (elapsed >= 5000) {
            const unsigned long rate =
                (static_cast<unsigned long>(device.activityWindowCount) *
                 60000UL) /
                elapsed;
            device.packetsPerMinute = static_cast<uint16_t>(
                rate > UINT16_MAX ? UINT16_MAX : rate);
        } else {
            device.packetsPerMinute = 0;
        }
    }

    if (observation.hasSSID && !observation.ssid.isEmpty()) {
        device.ssid = observation.ssid;
        device.hasSSID = true;
    }

    device.hasDistanceEstimate = distanceEstimator_.estimateMeters(
        device.filteredRSSI, device.distanceMeters);
    return true;
}

void DeviceManager::expire(unsigned long now) {
    size_t index = 0;
    while (index < deviceCount_) {
        if ((now - devices_[index].lastSeen) >= timeoutMs_) {
            removeAt(index);
        } else {
            ++index;
        }
    }
}

bool DeviceManager::setKnown(const uint8_t mac[6], bool known) {
    if (!isTrackableMac(mac)) {
        return false;
    }

    const int deviceIndex = findIndex(mac);
    if (known) {
        if (!rememberKnown(mac)) {
            return false;
        }
    } else {
        const int knownIndex = findKnownIndex(mac);
        if (knownIndex >= 0) {
            for (size_t i = static_cast<size_t>(knownIndex);
                 i + 1 < knownCount_; ++i) {
                memcpy(knownMacs_[i], knownMacs_[i + 1], 6);
            }
            --knownCount_;
        }
    }

    if (deviceIndex >= 0) {
        devices_[deviceIndex].known = known;
    }
    return true;
}

size_t DeviceManager::deviceCount() const {
    return deviceCount_;
}

const Device* DeviceManager::deviceAt(size_t index) const {
    return index < deviceCount_ ? &devices_[index] : nullptr;
}

const Device* DeviceManager::findByMac(const uint8_t mac[6]) const {
    if (!isTrackableMac(mac)) {
        return nullptr;
    }
    const int index = findIndex(mac);
    return index >= 0 ? &devices_[index] : nullptr;
}

bool DeviceManager::configureDistance(float rssiAtOneMeter,
                                      float pathLossExponent) {
    return distanceEstimator_.configure(rssiAtOneMeter, pathLossExponent);
}

unsigned long DeviceManager::timeoutMs() const {
    return timeoutMs_;
}

bool DeviceManager::isTrackableMac(const uint8_t mac[6]) {
    if (mac == nullptr || (mac[0] & 0x01) != 0) {
        return false;
    }

    for (size_t i = 0; i < 6; ++i) {
        if (mac[i] != 0) {
            return true;
        }
    }
    return false;
}

int DeviceManager::findIndex(const uint8_t mac[6]) const {
    for (size_t i = 0; i < deviceCount_; ++i) {
        if (memcmp(devices_[i].mac, mac, 6) == 0) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int DeviceManager::findKnownIndex(const uint8_t mac[6]) const {
    for (size_t i = 0; i < knownCount_; ++i) {
        if (memcmp(knownMacs_[i], mac, 6) == 0) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int DeviceManager::findOldestIndex(unsigned long now) const {
    size_t oldest = 0;
    for (size_t i = 1; i < deviceCount_; ++i) {
        if ((now - devices_[i].lastSeen) >
            (now - devices_[oldest].lastSeen)) {
            oldest = i;
        }
    }
    return static_cast<int>(oldest);
}

void DeviceManager::removeAt(size_t index) {
    if (index >= deviceCount_) {
        return;
    }

    for (size_t i = index; i + 1 < deviceCount_; ++i) {
        devices_[i] = devices_[i + 1];
        rssiFilters_[i] = rssiFilters_[i + 1];
    }
    --deviceCount_;
    devices_[deviceCount_] = Device{};
    rssiFilters_[deviceCount_].reset();
}

bool DeviceManager::isKnown(const uint8_t mac[6]) const {
    return findKnownIndex(mac) >= 0;
}

bool DeviceManager::rememberKnown(const uint8_t mac[6]) {
    if (findKnownIndex(mac) >= 0) {
        return true;
    }
    if (knownCount_ == kMaxDevices) {
        return false;
    }

    memcpy(knownMacs_[knownCount_++], mac, 6);
    return true;
}
