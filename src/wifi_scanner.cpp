#include "wifi_scanner.h"

WiFiScanner::WiFiScanner(unsigned long timeDiff)
    : scanIntervalMs_(timeDiff),
      lastScanMs_(0),
      hasScanned_(false),
      scanInProgress_(false),
      observationCount_(0) {
    for (size_t i = 0; i < kMaxObservations; ++i) {
        observations_[i] = RadioObservation{};
    }
}

bool WiFiScanner::begin() {
    if (!WiFi.mode(WIFI_STA)) {
        Serial.println("Unable to start WiFi station mode");
        return false;
    }
    if (!WiFi.setSleep(false)) {
        Serial.println("Unable to disable WiFi sleep");
        return false;
    }
    WiFi.disconnect();
    delay(100);
    hasScanned_ = false;
    scanInProgress_ = false;
    observationCount_ = 0;
    return true;
}

String WiFiScanner::formatMac(const uint8_t mac[6]) {
    char buffer[18];
    snprintf(buffer, sizeof(buffer), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(buffer);
}

void WiFiScanner::printObservation(const RadioObservation& observation) {
    Serial.printf("BSSID: %s | ", formatMac(observation.mac).c_str());

    if (observation.hasSSID && observation.ssid.length() > 0) {
        Serial.printf("SSID: %s | ", observation.ssid.c_str());
    } else {
        Serial.print("SSID: hidden or unavailable | ");
    }

    Serial.printf("RSSI: %d dBm | ", observation.rssi);
    Serial.printf("CH: %d | ", observation.channel);
    Serial.printf("TS: %lu ms\n", observation.timestamp);
}

bool WiFiScanner::collectResults(int foundNetworks) {
    observationCount_ = 0;
    if (foundNetworks < 0) {
        Serial.printf("WiFi scan failed (%d)\n", foundNetworks);
        WiFi.scanDelete();
        return false;
    }

    const unsigned long timestamp = millis();
    for (int i = 0; i < foundNetworks && i < kMaxObservations; ++i) {
        RadioObservation observation{};

        const uint8_t* bssid = WiFi.BSSID(i);
        if (bssid == nullptr) {
            continue;
        }
        memcpy(observation.mac, bssid, sizeof(observation.mac));

        observation.rssi = WiFi.RSSI(i);
        observation.channel = WiFi.channel(i);
        observation.timestamp = timestamp;

        const String ssid = WiFi.SSID(i);
        observation.hasSSID = !ssid.isEmpty();
        observation.ssid = ssid;
        observations_[observationCount_++] = observation;
    }

    WiFi.scanDelete();
    if (observationCount_ == 0) {
        Serial.println("No WiFi networks found");
    }
    return true;
}

bool WiFiScanner::update() {
    const unsigned long now = millis();
    if (scanInProgress_) {
        const int foundNetworks = WiFi.scanComplete();
        if (foundNetworks == WIFI_SCAN_RUNNING) {
            return false;
        }
        scanInProgress_ = false;
        return collectResults(foundNetworks);
    }

    if (!hasScanned_ || (now - lastScanMs_) >= scanIntervalMs_) {
        lastScanMs_ = now;
        hasScanned_ = true;
        observationCount_ = 0;
        const int status = WiFi.scanNetworks(true);
        if (status != WIFI_SCAN_RUNNING) {
            Serial.printf("Unable to start WiFi scan (%d)\n", status);
            WiFi.scanDelete();
            return false;
        }
        scanInProgress_ = true;
    }
    return false;
}

const RadioObservation* WiFiScanner::observations() const {
    return observations_;
}

size_t WiFiScanner::observationCount() const {
    return observationCount_;
}
