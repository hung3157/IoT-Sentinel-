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
    return sampleCount_ == 0 ? 0.0f : sum_ / sampleCount_;
}

float RssiFilter::trendSlope() const {
    if (sampleCount_ < 2) {
        return 0.0f;
    }

    const size_t firstIndex =
        sampleCount_ == kWindowSize ? nextIndex_ : 0;
    float sumY = 0.0f;
    float sumXY = 0.0f;
    for (size_t i = 0; i < sampleCount_; ++i) {
        const float y = samples_[(firstIndex + i) % kWindowSize];
        sumY += y;
        sumXY += static_cast<float>(i) * y;
    }

    const float count = static_cast<float>(sampleCount_);
    const float sumX = count * (count - 1.0f) / 2.0f;
    const float sumX2 =
        count * (count - 1.0f) * (2.0f * count - 1.0f) / 6.0f;
    const float denominator = count * sumX2 - sumX * sumX;
    return denominator == 0.0f
               ? 0.0f
               : (count * sumXY - sumX * sumY) / denominator;
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
