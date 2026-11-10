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
    if (device.rssiTrend == RssiTrend::Approaching) {
        score += 15;
    }
    if (device.packetsPerMinute >= 60) {
        score += 10;
    }

    if (score > 100) {
        score = 100;
    }

    RiskAssessment assessment{};
    assessment.score = static_cast<uint8_t>(score);
    assessment.level = score <= 30
                           ? RiskLevel::Low
                           : (score <= 60 ? RiskLevel::Medium
                                          : RiskLevel::High);
    return assessment;
}
