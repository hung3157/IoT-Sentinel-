#include <Arduino.h>

#include "button_input.h"
#include "channel_hopper.h"
#include "device_manager.h"
#include "oled_ui.h"
#include "risk_engine.h"
#include "wifi_scanner.h"
#include "wifi_sniffer.h"

#ifndef IOT_SENTINEL_USE_SNIFFER
#define IOT_SENTINEL_USE_SNIFFER 1
#endif

WiFiScanner wifiScanner(5000);
WiFiSniffer wifiSniffer;
ChannelHopper channelHopper(1, 11, 250);
DeviceManager deviceManager;
ButtonInput buttons(32, 33);
OledUi oledUi(21, 22);

static const uint8_t kAlertLedPin = 2;
static size_t selectedDevice = 0;
static UiMode uiMode = UiMode::List;
static bool snifferRunning = false;
static unsigned long lastStatusReportMs = 0;
static RiskEngine riskEngine;

static const char* trendName(RssiTrend trend) {
    if (trend == RssiTrend::Approaching) {
        return "APPROACHING";
    }
    if (trend == RssiTrend::MovingAway) {
        return "MOVING AWAY";
    }
    return "STABLE";
}

static const char* riskName(RiskLevel level) {
    if (level == RiskLevel::High) {
        return "HIGH";
    }
    if (level == RiskLevel::Medium) {
        return "MEDIUM";
    }
    return "LOW";
}

void setup() {
    Serial.begin(115200);
    delay(1000);
#if IOT_SENTINEL_USE_SNIFFER
    snifferRunning = wifiSniffer.begin();
    if (snifferRunning) {
        if (!channelHopper.begin(millis())) {
            Serial.println("Channel hopping unavailable; remaining on default channel");
        }
    } else {
        Serial.println("Sniffer initialization failed; falling back to WiFi scan");
        if (!wifiScanner.begin()) {
            Serial.println("WiFi scanner initialization failed");
        }
    }
#else
    if (!wifiScanner.begin()) {
        Serial.println("WiFi scanner initialization failed");
    }
#endif
    buttons.begin();
    pinMode(kAlertLedPin, OUTPUT);
    if (!oledUi.begin()) {
        Serial.println("OLED initialization failed; continuing without display");
    }
    Serial.println("\n**** IoT Sentinel started ****\n");
}

void loop() {
#if IOT_SENTINEL_USE_SNIFFER
    if (snifferRunning) {
        channelHopper.update(millis());
        RadioObservation observation;
        size_t processed = 0;
        while (processed < 16 && wifiSniffer.popObservation(observation)) {
            deviceManager.update(observation);
            ++processed;
        }
    } else if (wifiScanner.update()) {
        const RadioObservation* observations = wifiScanner.observations();
        for (size_t i = 0; i < wifiScanner.observationCount(); ++i) {
            deviceManager.update(observations[i]);
        }
    }
#else
    if (wifiScanner.update()) {
        const auto* observations = wifiScanner.observations();
        const size_t count = wifiScanner.observationCount();
        Serial.printf("\n=== %u NETWORK(S) FOUND ===\n",
                      static_cast<unsigned>(count));
        for (size_t i = 0; i < count; ++i) {
            WiFiScanner::printObservation(observations[i]);
            deviceManager.update(observations[i]);
        }
    }
#endif

    const unsigned long now = millis();
    deviceManager.expire(now);
    const ButtonEvents events = buttons.update(now);
    if (events.nextPressed && deviceManager.deviceCount() > 0) {
        selectedDevice =
            (selectedDevice + 1) % deviceManager.deviceCount();
    }
    if (events.modePressed) {
        uiMode = uiMode == UiMode::List ? UiMode::Detail : UiMode::List;
    }

    if (deviceManager.deviceCount() == 0) {
        selectedDevice = 0;
    } else if (selectedDevice >= deviceManager.deviceCount()) {
        selectedDevice = deviceManager.deviceCount() - 1;
    }

    bool highRiskPresent = false;
    for (size_t i = 0; i < deviceManager.deviceCount(); ++i) {
        const Device* device = deviceManager.deviceAt(i);
        if (riskEngine.evaluate(*device, now).level == RiskLevel::High) {
            highRiskPresent = true;
            break;
        }
    }
    digitalWrite(kAlertLedPin, highRiskPresent ? HIGH : LOW);
    oledUi.update(deviceManager, selectedDevice, uiMode, now);

    if ((now - lastStatusReportMs) >= 10000UL) {
#if IOT_SENTINEL_USE_SNIFFER
        if (snifferRunning) {
            Serial.printf("Sniffer queue drops: %lu\n",
                          static_cast<unsigned long>(
                              wifiSniffer.droppedCount()));
        }
#endif
        for (size_t i = 0; i < deviceManager.deviceCount(); ++i) {
            const Device* device = deviceManager.deviceAt(i);
            const RiskAssessment risk = riskEngine.evaluate(*device, now);
            Serial.printf(
                "%s RSSI raw=%d avg=%.1f dBm CH=%d packets=%lu "
                "rate=%u/min age=%lus trend=%s risk=%s(%u)\n",
                          formatMac(device->mac).c_str(),
                          device->rawRSSI, device->filteredRSSI,
                          device->channel,
                          static_cast<unsigned long>(device->packetCount),
                          device->packetsPerMinute,
                          (now - device->lastSeen) / 1000UL,
                          trendName(device->rssiTrend),
                          riskName(risk.level), risk.score);
        }
        lastStatusReportMs = now;
    }
}
