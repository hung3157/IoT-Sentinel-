#pragma once

#include "device_manager.h"

enum class RiskLevel : uint8_t {
    Low,
    Medium,
    High
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
