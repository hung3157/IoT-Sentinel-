#pragma once

#include <Arduino.h>

class RssiFilter {
public:
    static const size_t kWindowSize = 8;

    RssiFilter();

    float addSample(float sample);
    float value() const;
    float trendSlope() const;
    size_t sampleCount() const;
    void reset();

private:
    float samples_[kWindowSize];
    float sum_;
    size_t nextIndex_;
    size_t sampleCount_;
};
