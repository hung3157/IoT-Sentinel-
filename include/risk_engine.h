#pragma once

#include "device_manager.h"

enum RiskLevel {
    RISK_LOW,
    RISK_MEDIUM,
    RISK_HIGH
};

struct RiskAssessment {
    uint8_t score;
    RiskLevel level;
};

class RiskEngine {
public:
    RiskAssessment evaluate(const Device& device,
                            unsigned long now) const;
};
