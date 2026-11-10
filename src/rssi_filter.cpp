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

float RssiFilter::trendChange() const {
    if (sampleCount_ < 4) {
        return 0.0f;
    }

    const size_t firstIndex =
        sampleCount_ == kWindowSize ? nextIndex_ : 0;
    const size_t half = sampleCount_ / 2;
    float oldTotal = 0.0f;
    float newTotal = 0.0f;
    for (size_t i = 0; i < half; ++i) {
        oldTotal += samples_[(firstIndex + i) % kWindowSize];
        newTotal += samples_[(firstIndex + half + i) % kWindowSize];
    }
    return (newTotal - oldTotal) / static_cast<float>(half);
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
