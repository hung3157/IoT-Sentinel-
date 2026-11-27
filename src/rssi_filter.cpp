#include "rssi_filter.h"

RssiFilter::RssiFilter() : sum_(0.0f), nextIndex_(0), sampleCount_(0) {
    reset();
}

float RssiFilter::addSample(float sample) {
    if (sampleCount_ == kWindowSize) {
        sum_ -= samples_[nextIndex_];
    } else {
        ++sampleCount_;
    }

    samples_[nextIndex_] = sample;
    sum_ += sample;
    nextIndex_ = (nextIndex_ + 1) % kWindowSize;
    return value();
}

float RssiFilter::value() const {
    if (sampleCount_ == 0) {
        return 0.0f;
    }
    return sum_ / sampleCount_;
}

float RssiFilter::trendChange() const {
    if (sampleCount_ < 4) {
        return 0.0f;
    }

    size_t firstIndex = 0;
    if (sampleCount_ == kWindowSize) {
        firstIndex = nextIndex_;
    }
    const size_t oldCount = sampleCount_ / 2;
    const size_t newCount = sampleCount_ - oldCount;
    float oldTotal = 0.0f;
    float newTotal = 0.0f;
    for (size_t i = 0; i < oldCount; ++i) {
        oldTotal += samples_[(firstIndex + i) % kWindowSize];
    }
    for (size_t i = 0; i < newCount; ++i) {
        newTotal +=
            samples_[(firstIndex + oldCount + i) % kWindowSize];
    }
    const float oldAverage = oldTotal / (float)oldCount;
    const float newAverage = newTotal / (float)newCount;
    return newAverage - oldAverage;
}

size_t RssiFilter::sampleCount() const {
    return sampleCount_;
}

void RssiFilter::reset() {
    for (size_t i = 0; i < kWindowSize; ++i) {
        samples_[i] = 0.0f;
    }
    sum_ = 0.0f;
    nextIndex_ = 0;
    sampleCount_ = 0;
}
