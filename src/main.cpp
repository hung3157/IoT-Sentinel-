#include <Arduino.h>

#include "button_input.h"
#include "channel_hopper.h"
#include "device_manager.h"
#include "oled_ui.h"
#include "risk_engine.h"
#include "wifi_scanner.h"
#include "wifi_sniffer.h"

WiFiScanner wifiScanner(5000);
WiFiSniffer wifiSniffer;
ChannelHopper channelHopper(1, 11, 250);
DeviceManager deviceManager;
ButtonInput buttons(18, 19);
OledUi oledUi(21, 22);

static const uint8_t kAlertLedPin = 2;
// Set this to false to use periodic Wi-Fi scans instead.
static const bool useSniffer = true;
static size_t selectedDevice = 0;
static bool detailMode = false;
static bool snifferRunning = false;
static bool scannerRunning = false;
static unsigned long lastStatusReportMs = 0;
static unsigned long lastSimulatorReportMs = 0;
static RiskEngine riskEngine;
static char simulatorCommand[8];
static size_t simulatorCommandLength = 0;

static void startWifi() {
    if (useSniffer) {
        Serial.println("Starting passive WiFi sniffer...");
        snifferRunning = wifiSniffer.begin();
        if (snifferRunning && !channelHopper.begin(millis())) {
            Serial.println("Channel hopping unavailable; staying on one channel");
        }
        if (!snifferRunning) {
            Serial.println("Sniffer failed; trying WiFi scanner instead");
        }
    }

    if (!snifferRunning) {
        scannerRunning = wifiScanner.begin();
        if (!scannerRunning) {
            Serial.println("WiFi scanner initialization failed");
        } else {
            Serial.println("Using periodic WiFi scan");
        }
    }
}

static const char* trendName(RssiTrend trend) {
    if (trend == RSSI_APPROACHING) {
        return "APPROACHING";
    }
    if (trend == RSSI_MOVING_AWAY) {
        return "MOVING AWAY";
    }
    return "STABLE";
}

static const char* riskName(RiskLevel level) {
    if (level == RISK_HIGH) {
        return "HIGH";
    }
    if (level == RISK_MEDIUM) {
        return "MEDIUM";
    }
    return "LOW";
}

static String serialDeviceName(const Device& device) {
    if (!device.hasSSID) {
        return formatMac(device.mac);
    }

    String name;
    for (size_t i = 0; i < device.ssid.length() && i < 32; i++) {
        const char value = device.ssid[i];
        if (value >= 32 && value <= 126 && value != '|') {
            name += value;
        } else {
            name += '_';
        }
    }
    return name;
}

static void handleSimulatorCommand(char command) {
    if (command == 'R') {
        lastSimulatorReportMs = 0;
    } else if (command == 'M') {
        detailMode = !detailMode;
        lastSimulatorReportMs = 0;
    } else if (command == 'L') {
        detailMode = false;
        lastSimulatorReportMs = 0;
    } else if (command == 'N' && deviceManager.deviceCount() > 0) {
        selectedDevice++;
        if (selectedDevice >= deviceManager.deviceCount()) {
            selectedDevice = 0;
        }
        lastSimulatorReportMs = 0;
    } else if (command == 'P' && deviceManager.deviceCount() > 0) {
        if (selectedDevice == 0) {
            selectedDevice = deviceManager.deviceCount() - 1;
        } else {
            selectedDevice--;
        }
        lastSimulatorReportMs = 0;
    } else if (command == 'K' && deviceManager.deviceCount() > 0) {
        const Device* device = deviceManager.deviceAt(selectedDevice);
        deviceManager.setKnown(device->mac, !device->known);
        lastSimulatorReportMs = 0;
    } else if (command != 'R' && command != 'M' && command != 'L' &&
               command != 'N' && command != 'P' && command != 'K') {
        Serial.println("Unknown simulator command");
    }
}

static void readSimulatorCommands() {
    while (Serial.available() > 0) {
        const char value = (char)Serial.read();
        if (value == '\n' || value == '\r') {
            if (simulatorCommandLength > 0) {
                simulatorCommand[simulatorCommandLength] = '\0';
                handleSimulatorCommand(simulatorCommand[0]);
                simulatorCommandLength = 0;
            }
        } else if (simulatorCommandLength < sizeof(simulatorCommand) - 1) {
            simulatorCommand[simulatorCommandLength++] = value;
        }
    }
}

static void sendSimulatorData(unsigned long now) {
    if (now - lastSimulatorReportMs < 1000UL) {
        return;
    }

    Serial.printf("@SCREEN|%lu|%u|%u|%u\n",
                  now,
                  (unsigned)deviceManager.deviceCount(),
                  (unsigned)selectedDevice,
                  detailMode ? 1U : 0U);
    for (size_t i = 0; i < deviceManager.deviceCount(); i++) {
        const Device* device = deviceManager.deviceAt(i);
        const RiskAssessment risk = riskEngine.evaluate(*device, now);
        const String name = serialDeviceName(*device);
        Serial.printf(
            "@DEVICE|%u|%u|%s|%s|%d|%.1f|%d|%lu|%lu|%lu|%.1f|%s|%u|%s|%u|%u|%u|%lu|%lu|%lu\n",
            (unsigned)i,
            device->known ? 1U : 0U,
            formatMac(device->mac).c_str(),
            name.c_str(),
            device->rawRSSI,
            device->filteredRSSI,
            device->channel,
            (unsigned long)device->packetCount,
            device->seenCount,
            (now - device->lastSeen) / 1000UL,
            device->hasDistanceEstimate ? device->distanceMeters : -1.0f,
            trendName(device->rssiTrend),
            (unsigned)device->packetsPerMinute,
            riskName(risk.level),
            (unsigned)risk.score,
            (unsigned)device->lastFrameType,
            (unsigned)device->frameSubtype,
            (unsigned long)device->beaconCount,
            (unsigned long)device->probeRequestCount,
            (unsigned long)device->probeResponseCount);
    }
    Serial.println("@END");
    lastSimulatorReportMs = now;
}

static void readWifi() {
    if (snifferRunning) {
        channelHopper.update(millis());

        RadioObservation observation;
        int count = 0;
        while (count < 16 && wifiSniffer.popObservation(observation)) {
            deviceManager.update(observation);
            count++;
        }
    }

    if (scannerRunning && wifiScanner.update()) {
        const RadioObservation* observations = wifiScanner.observations();
        for (size_t i = 0; i < wifiScanner.observationCount(); i++) {
            deviceManager.update(observations[i]);
            WiFiScanner::printObservation(observations[i]);
        }
    }
}

static void updateButtons(unsigned long now) {
    const ButtonEvents events = buttons.update(now);

    if (events.nextPressed && deviceManager.deviceCount() > 0) {
        selectedDevice++;
        if (selectedDevice >= deviceManager.deviceCount()) {
            selectedDevice = 0;
        }
    }

    if (events.modePressed) {
        detailMode = !detailMode;
    }

    if (deviceManager.deviceCount() == 0) {
        selectedDevice = 0;
    } else if (selectedDevice >= deviceManager.deviceCount()) {
        selectedDevice = deviceManager.deviceCount() - 1;
    }
}

static void updateAlert(unsigned long now) {
    bool highRiskFound = false;

    for (size_t i = 0; i < deviceManager.deviceCount(); i++) {
        const Device* device = deviceManager.deviceAt(i);
        const RiskAssessment risk = riskEngine.evaluate(*device, now);
        if (risk.level == RISK_HIGH) {
            highRiskFound = true;
            break;
        }
    }

    if (highRiskFound) {
        digitalWrite(kAlertLedPin, HIGH);
    } else {
        digitalWrite(kAlertLedPin, LOW);
    }
}

static void printStatus(unsigned long now) {
    if (now - lastStatusReportMs < 10000UL) {
        return;
    }

    if (snifferRunning) {
        Serial.printf("Sniffer queue drops: %lu\n",
                      (unsigned long)wifiSniffer.droppedCount());
    }

    for (size_t i = 0; i < deviceManager.deviceCount(); i++) {
        const Device* device = deviceManager.deviceAt(i);
        const RiskAssessment risk = riskEngine.evaluate(*device, now);
        Serial.printf(
            "%s RSSI raw=%d avg=%.1f dBm CH=%d packets=%lu "
            "rate=%u/min age=%lus trend=%s risk=%s(%u)\n",
            formatMac(device->mac).c_str(),
            device->rawRSSI, device->filteredRSSI,
            device->channel,
            (unsigned long)device->packetCount,
            device->packetsPerMinute,
            (now - device->lastSeen) / 1000UL,
            trendName(device->rssiTrend),
            riskName(risk.level), risk.score);
    }

    lastStatusReportMs = now;
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    startWifi();
    buttons.begin();
    pinMode(kAlertLedPin, OUTPUT);
    if (!oledUi.begin()) {
        Serial.println("OLED initialization failed; continuing without display");
    }
    Serial.println("\n**** IoT Sentinel started ****\n");
}

void loop() {
    readSimulatorCommands();
    readWifi();

    const unsigned long now = millis();
    deviceManager.expire(now);
    updateButtons(now);
    updateAlert(now);
    oledUi.update(deviceManager, selectedDevice, detailMode, now);
    sendSimulatorData(now);
    printStatus(now);
}
