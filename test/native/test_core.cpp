#include <assert.h>
#include <math.h>
#include <stdint.h>

#include "device_manager.h"
#include "distance_estimator.h"
#include "rssi_filter.h"
#include "risk_engine.h"
#include "semiconductor_diagnostics.h"

static bool near(float actual, float expected, float tolerance = 0.01f) {
    return fabsf(actual - expected) <= tolerance;
}

static RadioObservation observation(uint8_t macSuffix, int rssi,
                                    unsigned long timestamp,
                                    RadioFrameType frameType =
                                        RadioFrameType::Unknown) {
    RadioObservation result{};
    result.mac[0] = 0x02;
    result.mac[5] = macSuffix;
    result.rssi = rssi;
    result.channel = 6;
    result.timestamp = timestamp;
    result.frameType = frameType;
    return result;
}

int main() {
    RssiFilter filter;
    assert(near(filter.addSample(-50.0f), -50.0f));
    assert(near(filter.addSample(-54.0f), -52.0f));
    filter.reset();
    for (int i = 0; i < 8; ++i) {
        filter.addSample(static_cast<float>(-60 + i));
    }
    assert(filter.sampleCount() == 8);
    assert(filter.trendChange() > 3.0f);
    assert(near(filter.addSample(-52.0f), -55.5f));

    DistanceEstimator estimator;
    float distance = 0.0f;
    assert(estimator.configure(-50.0f, 2.0f));
    assert(estimator.estimateMeters(-50.0f, distance));
    assert(near(distance, 1.0f));
    assert(estimator.estimateMeters(-70.0f, distance));
    assert(near(distance, 10.0f));
    assert(!estimator.configure(-50.0f, 0.0f));

    DeviceManager devices(5000);
    RadioObservation first = observation(1, -60, 1000,
                                        RadioFrameType::Data);
    assert(devices.update(first));
    assert(devices.update(observation(1, -59, 2000,
                                     RadioFrameType::Management)));
    assert(devices.deviceCount() == 1);
    const Device* tracked = devices.findByMac(first.mac);
    assert(tracked != nullptr);
    assert(tracked->seenCount == 2);
    assert(tracked->packetCount == 2);
    assert(tracked->dataFrameCount == 1);
    assert(tracked->managementFrameCount == 1);

    for (unsigned long time = 3000; time <= 7000; time += 1000) {
        assert(devices.update(observation(
            1, -58 + static_cast<int>((time - 3000) / 1000), time,
            RadioFrameType::Data)));
    }
    tracked = devices.findByMac(first.mac);
    assert(tracked->packetsPerMinute >= 60);
    assert(tracked->rssiTrend == RssiTrend::Approaching);

    assert(devices.setKnown(first.mac, true));
    assert(devices.findByMac(first.mac)->known);
    devices.expire(11999);
    assert(devices.deviceCount() == 1);
    devices.expire(12000);
    assert(devices.deviceCount() == 0);

    RadioObservation multicast = observation(2, -40, 9000);
    multicast.mac[0] = 0x01;
    assert(!devices.update(multicast));
    assert(devices.deviceCount() == 0);

    DeviceManager full(100000);
    for (uint8_t i = 1; i <= DeviceManager::kMaxDevices; ++i) {
        assert(full.update(observation(i, -60, 1000)));
    }
    assert(full.deviceCount() == DeviceManager::kMaxDevices);
    assert(full.update(observation(33, -50, 1001)));
    assert(full.deviceCount() == DeviceManager::kMaxDevices);
    assert(full.findByMac(observation(1, -60, 1000).mac) == nullptr);
    assert(full.findByMac(observation(33, -50, 1001).mac) != nullptr);

    Device riskDevice{};
    riskDevice.filteredRSSI = -50.0f;
    riskDevice.seenCount = 40;
    riskDevice.firstSeen = 1000;
    riskDevice.rssiTrend = RssiTrend::Approaching;
    riskDevice.packetsPerMinute = 100;
    RiskAssessment risk = RiskEngine().evaluate(riskDevice, 61000);
    assert(risk.score == 100);
    assert(risk.level == RiskLevel::High);
    riskDevice.known = true;
    riskDevice.filteredRSSI = -80.0f;
    riskDevice.seenCount = 1;
    riskDevice.firstSeen = 61000;
    riskDevice.rssiTrend = RssiTrend::Stable;
    riskDevice.packetsPerMinute = 0;
    risk = RiskEngine().evaluate(riskDevice, 61000);
    assert(risk.score == 0);
    assert(risk.level == RiskLevel::Low);

    BjtAnalysis bjt{};
    assert(SemiconductorDiagnostics::analyzeNpn(
        0.7f, 2.0f, 0.0f, 3.3f, 1000.0f, bjt));
    assert(bjt.region == BjtRegion::Active);
    assert(near(bjt.vbe, 0.7f));
    assert(near(bjt.vce, 2.0f));
    assert(near(bjt.collectorCurrentAmps, 0.0013f, 0.00001f));
    assert(SemiconductorDiagnostics::analyzeNpn(
        0.7f, 0.1f, 0.0f, 3.3f, 1000.0f, bjt));
    assert(bjt.region == BjtRegion::Saturation);
    assert(SemiconductorDiagnostics::analyzeNpn(
        0.4f, 3.2f, 0.0f, 3.3f, 1000.0f, bjt));
    assert(bjt.region == BjtRegion::Cutoff);
    assert(!SemiconductorDiagnostics::analyzeNpn(
        0.7f, 3.5f, 0.0f, 3.3f, 1000.0f, bjt));

    NmosRegion mosfetRegion = NmosRegion::Cutoff;
    assert(SemiconductorDiagnostics::analyzeNmos(1.0f, 1.0f, 2.0f,
                                                 mosfetRegion));
    assert(mosfetRegion == NmosRegion::Cutoff);
    assert(SemiconductorDiagnostics::analyzeNmos(3.0f, 0.5f, 2.0f,
                                                 mosfetRegion));
    assert(mosfetRegion == NmosRegion::Linear);
    assert(SemiconductorDiagnostics::analyzeNmos(3.0f, 2.0f, 2.0f,
                                                 mosfetRegion));
    assert(mosfetRegion == NmosRegion::Saturation);

    DiodeAnalysis diode{};
    assert(SemiconductorDiagnostics::analyzeDiode(0.65f, 0.01f, diode));
    assert(diode.conducting);
    assert(!SemiconductorDiagnostics::analyzeDiode(-0.1f, 0.0f, diode));
    return 0;
}
