#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "device_manager.h"
#include "distance_estimator.h"
#include "risk_engine.h"
#include "rssi_filter.h"

static unsigned int checks = 0;
static unsigned int failures = 0;

static void check(bool passed, const char* message) {
    checks++;
    if (!passed) {
        failures++;
        printf("FAIL: %s\n", message);
    }
}

static bool near(float actual, float expected, float tolerance = 0.01f) {
    return fabsf(actual - expected) <= tolerance;
}

static RadioObservation observation(uint8_t macSuffix, int rssi,
                                    unsigned long timestamp,
                                    RadioFrameType frameType = FRAME_UNKNOWN) {
    RadioObservation result{};
    result.mac[0] = 0x02;
    result.mac[5] = macSuffix;
    result.rssi = rssi;
    result.channel = 6;
    result.timestamp = timestamp;
    result.frameType = frameType;
    return result;
}

static void testRssiFilter() {
    RssiFilter filter;
    check(near(filter.value(), 0.0f), "empty RSSI filter returns zero");
    check(near(filter.addSample(-50.0f), -50.0f),
          "first RSSI sample is returned");
    check(near(filter.addSample(-54.0f), -52.0f),
          "RSSI filter averages samples");

    filter.reset();
    check(filter.sampleCount() == 0, "RSSI reset clears sample count");
    for (int i = 0; i < 8; ++i) {
        filter.addSample((float)(-60 + i));
    }
    check(filter.sampleCount() == 8, "RSSI filter keeps its window full");
    check(filter.trendChange() > 3.0f,
          "increasing RSSI produces a positive trend");
    check(near(filter.addSample(-52.0f), -55.5f),
          "full RSSI window replaces its oldest sample");

    RssiFilter awayFilter;
    RssiFilter steadyFilter;
    for (int i = 0; i < 8; ++i) {
        awayFilter.addSample((float)(-50 - i));
        steadyFilter.addSample(-60.0f);
    }
    check(awayFilter.trendChange() < -3.0f,
          "decreasing RSSI produces a negative trend");
    check(near(steadyFilter.trendChange(), 0.0f),
          "steady RSSI produces no trend");

    RssiFilter oddFilter;
    for (int i = 0; i < 4; ++i) {
        oddFilter.addSample(-60.0f);
    }
    oddFilter.addSample(-40.0f);
    check(oddFilter.trendChange() > 5.0f,
          "trend handles an odd number of samples");
}

static void testDistanceEstimator() {
    DistanceEstimator estimator;
    float distance = 0.0f;
    check(estimator.configure(-50.0f, 2.0f),
          "valid distance settings are accepted");
    check(estimator.estimateMeters(-50.0f, distance),
          "distance is estimated at the reference RSSI");
    check(near(distance, 1.0f), "reference RSSI estimates one meter");
    check(estimator.estimateMeters(-70.0f, distance),
          "distance is estimated for weaker RSSI");
    check(near(distance, 10.0f), "weaker RSSI estimates ten meters");

    check(!estimator.configure(-50.0f, 0.0f),
          "zero path loss exponent is rejected");
    check(!estimator.configure(-50.0f, NAN),
          "NaN path loss exponent is rejected");
    check(estimator.estimateMeters(-70.0f, distance) &&
              near(distance, 10.0f),
          "invalid settings do not replace valid settings");
    check(!estimator.estimateMeters(NAN, distance),
          "NaN RSSI is rejected");
}

static void testDeviceTracking() {
    DeviceManager devices(5000);
    RadioObservation first =
        observation(1, -60, 1000, FRAME_DATA);
    check(devices.update(first), "first valid observation is accepted");

    RadioObservation second =
        observation(1, -59, 2000, FRAME_MANAGEMENT);
    second.frameSubtype = 8;
    second.hasSSID = true;
    second.ssid = "TestNet";
    check(devices.update(second), "second device observation is accepted");
    check(devices.deviceCount() == 1, "same MAC stays one tracked device");

    const Device* tracked = devices.findByMac(first.mac);
    check(tracked != nullptr, "device can be found by MAC");
    if (tracked == nullptr) {
        return;
    }
    check(tracked->seenCount == 2, "seen count increments per observation");
    check(tracked->packetCount == 2, "packet count increments for frames");
    check(tracked->dataFrameCount == 1, "data frame count is recorded");
    check(tracked->managementFrameCount == 1,
          "management frame count is recorded");
    check(tracked->lastFrameType == FRAME_MANAGEMENT,
          "most recent frame type is recorded");
    check(tracked->frameSubtype == 8,
          "most recent frame subtype is recorded");
    check(tracked->beaconCount == 1,
          "beacon subtype count is recorded");
    check(tracked->hasSSID &&
              strcmp(tracked->ssid.c_str(), "TestNet") == 0,
          "network name is saved when available");
    check(devices.deviceAt(1) == nullptr,
          "out-of-range device index returns null");

    for (unsigned long time = 3000; time <= 7000; time += 1000) {
        const int rssi = -59 + (int)((time - 2000) / 1000);
        check(devices.update(
                  observation(1, rssi, time, FRAME_DATA)),
              "later observation updates the tracked device");
    }
    tracked = devices.findByMac(first.mac);
    check(tracked != nullptr, "updated device remains findable");
    if (tracked != nullptr) {
        check(tracked->packetsPerMinute >= 60,
              "packet activity rate is calculated after five seconds");
        check(tracked->rssiTrend == RSSI_APPROACHING,
              "increasing RSSI marks device as approaching");
    }

    RadioObservation probeRequest = observation(1, -56, 8000, FRAME_MANAGEMENT);
    probeRequest.frameSubtype = 4;
    check(devices.update(probeRequest), "probe request observation is accepted");
    RadioObservation probeResponse = observation(1, -56, 9000, FRAME_MANAGEMENT);
    probeResponse.frameSubtype = 5;
    check(devices.update(probeResponse), "probe response observation is accepted");
    tracked = devices.findByMac(first.mac);
    if (tracked != nullptr) {
        check(tracked->beaconCount == 1 &&
                  tracked->probeRequestCount == 1 &&
                  tracked->probeResponseCount == 1,
              "management subtype evidence is counted separately");
    }

    check(devices.setKnown(first.mac, true),
          "device can be marked known");
    check(devices.setKnown(first.mac, true),
          "marking a device known twice succeeds");
    tracked = devices.findByMac(first.mac);
    check(tracked != nullptr && tracked->known,
          "known state is visible on the tracked device");
    check(devices.setKnown(first.mac, false),
          "device can be marked unknown");
    tracked = devices.findByMac(first.mac);
    check(tracked != nullptr && !tracked->known,
          "unknown state is visible on the tracked device");

    check(devices.setKnown(first.mac, true),
          "device can be marked known again");
    devices.expire(13999);
    check(devices.deviceCount() == 1,
          "device remains before its timeout boundary");
    devices.expire(14000);
    check(devices.deviceCount() == 0,
          "device expires at its timeout boundary");
    check(devices.setKnown(first.mac, true),
          "known MAC can be saved after device expiration");
    check(devices.update(observation(1, -55, 15000, FRAME_DATA)),
          "expired device can be detected again");
    tracked = devices.findByMac(first.mac);
    check(tracked != nullptr && tracked->known,
          "known state survives device expiration");

    RadioObservation multicast = observation(2, -40, 16000);
    multicast.mac[0] = 0x01;
    check(!devices.update(multicast), "multicast MAC is rejected");
    RadioObservation zeroMac{};
    zeroMac.rssi = -40;
    zeroMac.timestamp = 16000;
    check(!devices.update(zeroMac), "all-zero MAC is rejected");
}

static void testDeviceCapacityAndActivity() {
    DeviceManager full(100000);
    for (uint8_t i = 1; i <= DeviceManager::kMaxDevices; ++i) {
        check(full.update(observation(i, -60, 1000)),
              "device is added while there is available capacity");
    }
    check(full.deviceCount() == DeviceManager::kMaxDevices,
          "device list stops at its maximum size");
    check(full.update(observation(33, -50, 1001)),
          "new device is accepted when list is full");
    check(full.deviceCount() == DeviceManager::kMaxDevices,
          "new device replaces one entry instead of growing the list");
    check(full.findByMac(observation(1, -60, 1000).mac) == nullptr,
          "oldest device is replaced when list is full");
    check(full.findByMac(observation(33, -50, 1001).mac) != nullptr,
          "new device is present after replacement");

    DeviceManager activity(120000);
    for (unsigned long time = 1000; time <= 7000; time += 1000) {
        check(activity.update(observation(1, -60, time, FRAME_DATA)),
              "activity observation is accepted");
    }
    const Device* device = activity.deviceAt(0);
    check(device != nullptr && device->packetsPerMinute >= 60,
          "activity rate becomes available after five seconds");
    activity.expire(61000);
    device = activity.deviceAt(0);
    check(device != nullptr && device->packetsPerMinute == 0,
          "packet rate clears after its activity window expires");
}

static void testRiskEngine() {
    RiskEngine engine;
    Device device{};
    device.filteredRSSI = -80.0f;
    device.firstSeen = 61000;
    RiskAssessment risk = engine.evaluate(device, 61000);
    check(risk.score == 30 && risk.level == RISK_LOW,
          "unknown device alone has low risk");

    device.seenCount = 30;
    device.packetsPerMinute = 60;
    risk = engine.evaluate(device, 61000);
    check(risk.score == 60 && risk.level == RISK_MEDIUM,
          "score of sixty is medium risk");

    device.filteredRSSI = -55.0f;
    risk = engine.evaluate(device, 61000);
    check(risk.score == 85 && risk.level == RISK_HIGH,
          "score above sixty is high risk");

    device.known = true;
    device.filteredRSSI = -80.0f;
    device.seenCount = 1;
    device.firstSeen = 61000;
    device.packetsPerMinute = 0;
    device.rssiTrend = RSSI_STABLE;
    risk = engine.evaluate(device, 61000);
    check(risk.score == 0 && risk.level == RISK_LOW,
          "known quiet device has zero risk");

    device.known = false;
    device.filteredRSSI = -50.0f;
    risk = engine.evaluate(device, 61000);
    check(risk.score == 55 && risk.level == RISK_MEDIUM,
          "unknown nearby device receives expected score");

    device.seenCount = 40;
    device.rssiTrend = RSSI_APPROACHING;
    device.packetsPerMinute = 100;
    risk = engine.evaluate(device, 61000);
    check(risk.score == 100 && risk.level == RISK_HIGH,
          "risk score is capped at one hundred");
}

int main() {
    testRssiFilter();
    testDistanceEstimator();
    testDeviceTracking();
    testDeviceCapacityAndActivity();
    testRiskEngine();

    if (failures != 0) {
        printf("%u of %u checks failed.\n", failures, checks);
        return 1;
    }

    printf("All %u core checks passed.\n", checks);
    return 0;
}
