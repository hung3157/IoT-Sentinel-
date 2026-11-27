#include "risk_engine.h"

RiskAssessment RiskEngine::evaluate(const Device& device,
                                    unsigned long now) const {
    unsigned int score = 0;
    if (!device.known) {
        score += 30;
    }
    if (device.filteredRSSI >= -55.0f) {
        score += 25;
    }
    if (device.seenCount >= 30 ||
        (now - device.firstSeen) >= 60000UL) {
        score += 20;
    }
    if (device.rssiTrend == RSSI_APPROACHING) {
        score += 15;
    }
    if (device.packetsPerMinute >= 60) {
        score += 10;
    }

    if (score > 100) {
        score = 100;
    }

    RiskAssessment assessment{};
    assessment.score = (uint8_t)score;
    if (score <= 30) {
        assessment.level = RISK_LOW;
    } else if (score <= 60) {
        assessment.level = RISK_MEDIUM;
    } else {
        assessment.level = RISK_HIGH;
    }
    return assessment;
}
