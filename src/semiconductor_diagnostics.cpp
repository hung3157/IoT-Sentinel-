#include "semiconductor_diagnostics.h"

#include <math.h>

bool SemiconductorDiagnostics::analyzeNpn(
    float baseVoltage, float collectorVoltage, float emitterVoltage,
    float supplyVoltage, float collectorResistanceOhms,
    BjtAnalysis& analysis) {
    if (!isfinite(baseVoltage) || !isfinite(collectorVoltage) ||
        !isfinite(emitterVoltage) || !isfinite(supplyVoltage) ||
        !isfinite(collectorResistanceOhms) || supplyVoltage <= 0.0f ||
        collectorResistanceOhms <= 0.0f || baseVoltage < 0.0f ||
        baseVoltage > supplyVoltage || emitterVoltage < 0.0f ||
        collectorVoltage < emitterVoltage ||
        collectorVoltage > supplyVoltage) {
        return false;
    }

    BjtAnalysis result{};
    result.vbe = baseVoltage - emitterVoltage;
    result.vce = collectorVoltage - emitterVoltage;
    result.collectorCurrentAmps =
        (supplyVoltage - collectorVoltage) / collectorResistanceOhms;
    result.region = result.vbe < 0.55f
                        ? BjtRegion::Cutoff
                        : (result.vce > 0.2f ? BjtRegion::Active
                                             : BjtRegion::Saturation);
    analysis = result;
    return true;
}

bool SemiconductorDiagnostics::analyzeNmos(
    float gateSourceVoltage, float drainSourceVoltage,
    float thresholdVoltage, NmosRegion& region) {
    if (!isfinite(gateSourceVoltage) || !isfinite(drainSourceVoltage) ||
        !isfinite(thresholdVoltage) || drainSourceVoltage < 0.0f ||
        thresholdVoltage < 0.0f) {
        return false;
    }

    region = gateSourceVoltage < thresholdVoltage
                 ? NmosRegion::Cutoff
                 : (drainSourceVoltage <
                            (gateSourceVoltage - thresholdVoltage)
                        ? NmosRegion::Linear
                        : NmosRegion::Saturation);
    return true;
}

bool SemiconductorDiagnostics::analyzeDiode(
    float forwardVoltage, float currentAmps, DiodeAnalysis& analysis) {
    if (!isfinite(forwardVoltage) || !isfinite(currentAmps) ||
        forwardVoltage < 0.0f || currentAmps < 0.0f) {
        return false;
    }

    analysis.forwardVoltage = forwardVoltage;
    analysis.currentAmps = currentAmps;
    analysis.conducting = currentAmps > 0.0f;
    return true;
}
