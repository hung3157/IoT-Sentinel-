#pragma once

#include <Adafruit_SSD1306.h>

#include "device_manager.h"
#include "risk_engine.h"

enum class UiMode : uint8_t {
    List,
    Detail,
    Diagnostics
};

class OledUi {
public:
    OledUi(uint8_t sdaPin, uint8_t sclPin, uint8_t address = 0x3C);

    bool begin();
    void update(const DeviceManager& devices, size_t selectedIndex,
                UiMode mode, unsigned long now);

private:
    void renderList(const DeviceManager& devices, size_t selectedIndex);
    void renderDetail(const Device& device, unsigned long now);
    void renderDiagnostics();
    static const char* riskLabel(RiskLevel level);
    static const char* trendLabel(RssiTrend trend);

    Adafruit_SSD1306 display_;
    uint8_t sdaPin_;
    uint8_t sclPin_;
    uint8_t address_;
    unsigned long lastRenderMs_;
    bool initialized_;
};
