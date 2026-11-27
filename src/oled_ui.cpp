#include "oled_ui.h"

#include <Wire.h>

OledUi::OledUi(uint8_t sdaPin, uint8_t sclPin, uint8_t address)
    : display_(128, 64, &Wire, -1),
      sdaPin_(sdaPin),
      sclPin_(sclPin),
      address_(address),
      lastRenderMs_(0),
      initialized_(false) {}

bool OledUi::begin() {
    Wire.begin(sdaPin_, sclPin_);
    initialized_ = display_.begin(SSD1306_SWITCHCAPVCC, address_);
    if (!initialized_) {
        return false;
    }
    display_.clearDisplay();
    display_.setTextColor(SSD1306_WHITE);
    display_.setTextSize(1);
    display_.display();
    return true;
}

void OledUi::update(const DeviceManager& devices, size_t selectedIndex,
                    bool detailMode, unsigned long now) {
    if (!initialized_ || (now - lastRenderMs_) < 250) {
        return;
    }
    lastRenderMs_ = now;
    display_.clearDisplay();

    if (detailMode && devices.deviceCount() > 0) {
        size_t index = selectedIndex;
        if (index >= devices.deviceCount()) {
            index = 0;
        }
        renderDetail(*devices.deviceAt(index), now);
    } else {
        renderList(devices, selectedIndex);
    }

    display_.display();
}

void OledUi::renderList(const DeviceManager& devices, size_t selectedIndex) {
    display_.setCursor(0, 0);
    display_.println("IOT SENTINEL");
    display_.drawFastHLine(0, 10, 128, SSD1306_WHITE);

    if (devices.deviceCount() == 0) {
        display_.setCursor(0, 26);
        display_.println("Scanning...");
        return;
    }

    size_t start = 0;
    if (selectedIndex >= 3) {
        start = selectedIndex - 2;
    }
    int16_t y = 14;
    for (size_t row = 0; row < 3 && start + row < devices.deviceCount();
         ++row) {
        const Device* device = devices.deviceAt(start + row);
        display_.setCursor(0, y);
        if (start + row == selectedIndex) {
            display_.print(">");
        } else {
            display_.print(" ");
        }
        if (device->known) {
            display_.print("KNOWN ");
        } else {
            display_.print("UNKNOWN ");
        }
        if (device->hasSSID) {
            display_.print(device->ssid.substring(0, 8));
        } else {
            display_.print(formatMac(device->mac).substring(9));
        }
        display_.setCursor(7, y + 8);
        display_.printf("RSSI %d dBm", device->rawRSSI);
        y += 16;
    }
}

void OledUi::renderDetail(const Device& device, unsigned long now) {
    const RiskAssessment risk = RiskEngine().evaluate(device, now);
    display_.setCursor(0, 0);
    if (device.known) {
        display_.println("KNOWN DEVICE");
    } else {
        display_.println("UNKNOWN DEVICE");
    }
    display_.setCursor(0, 12);
    const String mac = formatMac(device.mac);
    display_.printf("MAC %.8s", mac.c_str());
    display_.setCursor(0, 22);
    display_.printf("    %.9s CH %d", mac.c_str() + 9, device.channel);
    display_.setCursor(0, 32);
    display_.printf("RSSI %d / %.1f", device.rawRSSI,
                    device.filteredRSSI);
    display_.setCursor(0, 42);
    if (device.hasDistanceEstimate) {
        display_.printf("D~%.1fm P%lu", device.distanceMeters,
                        (unsigned long)device.packetCount);
    } else {
        display_.printf("DIST -- P%lu",
                        (unsigned long)device.packetCount);
    }
    display_.setCursor(0, 52);
    display_.printf("RISK %s %s", riskLabel(risk.level),
                    trendLabel(device.rssiTrend));
}

const char* OledUi::riskLabel(RiskLevel level) {
    switch (level) {
        case RISK_LOW:
            return "LOW";
        case RISK_MEDIUM:
            return "MED";
        case RISK_HIGH:
            return "HIGH";
    }
    return "UNKNOWN";
}

const char* OledUi::trendLabel(RssiTrend trend) {
    switch (trend) {
        case RSSI_APPROACHING:
            return "APPROACH";
        case RSSI_STABLE:
            return "STABLE";
        case RSSI_MOVING_AWAY:
            return "AWAY";
    }
    return "UNKNOWN";
}
