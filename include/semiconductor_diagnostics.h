#pragma once

#include <stdint.h>

enum class BjtRegion : uint8_t {
    Cutoff,
    Active,
    Saturation
};

struct BjtAnalysis {
    float vbe;
    float vce;
    float collectorCurrentAmps;
    BjtRegion region;
};

enum class NmosRegion : uint8_t {
    Cutoff,
    Linear,
    Saturation
};

struct DiodeAnalysis {
    float forwardVoltage;
    float currentAmps;
    bool conducting;
};

class SemiconductorDiagnostics {
public:
    static bool analyzeNpn(float baseVoltage, float collectorVoltage,
                           float emitterVoltage, float supplyVoltage,
                           float collectorResistanceOhms,
                           BjtAnalysis& analysis);
    static bool analyzeNmos(float gateSourceVoltage,
                            float drainSourceVoltage,
                            float thresholdVoltage, NmosRegion& region);
    static bool analyzeDiode(float forwardVoltage, float currentAmps,
                             DiodeAnalysis& analysis);
};
