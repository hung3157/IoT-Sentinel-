#pragma once

class DistanceEstimator {
public:
    DistanceEstimator();

    bool configure(float rssiAtOneMeter, float pathLossExponent);
    bool estimateMeters(float filteredRssi, float& distanceMeters) const;

private:
    float rssiAtOneMeter_;
    float pathLossExponent_;
    bool configured_;
};
