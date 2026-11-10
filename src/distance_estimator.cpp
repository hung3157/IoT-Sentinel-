#include "distance_estimator.h"

#include <math.h>

DistanceEstimator::DistanceEstimator()
    : rssiAtOneMeter_(-50.0f),
      pathLossExponent_(2.0f),
      configured_(true) {}

bool DistanceEstimator::configure(float rssiAtOneMeter,
                                  float pathLossExponent) {
    if (!isfinite(rssiAtOneMeter) || !isfinite(pathLossExponent) ||
        pathLossExponent <= 0.0f) {
        return false;
    }

    rssiAtOneMeter_ = rssiAtOneMeter;
    pathLossExponent_ = pathLossExponent;
    configured_ = true;
    return true;
}

bool DistanceEstimator::estimateMeters(float filteredRssi,
                                       float& distanceMeters) const {
    if (!configured_ || !isfinite(filteredRssi)) {
        return false;
    }

    const float exponent =
        (rssiAtOneMeter_ - filteredRssi) / (10.0f * pathLossExponent_);
    const float estimate = powf(10.0f, exponent);
    if (!isfinite(estimate) || estimate <= 0.0f) {
        return false;
    }

    distanceMeters = estimate;
    return true;
}
