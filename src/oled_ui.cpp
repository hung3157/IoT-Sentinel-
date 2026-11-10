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
                    UiMode mode, unsigned long now) {
    if (!initialized_ || (now - lastRenderMs_) < 250) {
        return;
    }
    lastRenderMs_ = now;
    display_.clearDisplay();

    if (mode == UiMode::Detail && devices.deviceCount() > 0) {
        const size_t index =
            selectedIndex < devices.deviceCount() ? selectedIndex : 0;
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

    const size_t start = selectedIndex < 3 ? 0 : selectedIndex - 2;
    for (size_t row = 0; row < 3 && start + row < devices.deviceCount();
         ++row) {
        const Device* device = devices.deviceAt(start + row);
        display_.setCursor(0, 14 + static_cast<int16_t>(row * 16));
        display_.print(start + row == selectedIndex ? ">" : " ");
        display_.print(device->known ? "KNOWN " : "UNKNOWN ");
        if (device->hasSSID) {
            display_.print(device->ssid.substring(0, 8));
        } else {
            display_.print(formatMac(device->mac).substring(9));
        }
        display_.setCursor(7, 22 + static_cast<int16_t>(row * 16));
        display_.printf("RSSI %d dBm", device->rawRSSI);
    }
}

void OledUi::renderDetail(const Device& device, unsigned long now) {
    const RiskAssessment risk = RiskEngine().evaluate(device, now);
    display_.setCursor(0, 0);
    display_.println(device.known ? "KNOWN DEVICE" : "UNKNOWN DEVICE");
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
        display_.printf("DIST EST ~%.1f m", device.distanceMeters);
    } else {
        display_.print("DIST EST unavailable");
    }
    display_.setCursor(0, 52);
    display_.printf("RISK %s SEEN %lu", riskLabel(risk.level),
                    device.seenCount);
}

const char* OledUi::riskLabel(RiskLevel level) {
    switch (level) {
        case RiskLevel::Low:
            return "LOW";
        case RiskLevel::Medium:
            return "MED";
        case RiskLevel::High:
            return "HIGH";
    }
    return "UNKNOWN";
}

const char* OledUi::trendLabel(RssiTrend trend) {
    switch (trend) {
        case RssiTrend::Approaching:
            return "APPROACH";
        case RssiTrend::Stable:
            return "STABLE";
        case RssiTrend::MovingAway:
            return "AWAY";
    }
    return "UNKNOWN";
}
