#pragma once
#include <cmath>
#include <algorithm>

// Zero-alloc auto-gain for honest A/B testing:
// - Call reset(fs) in prepareToPlay
// - Each block: compute linear makeup = AutoGain::compute(rmsProcessed, rmsDry)
// - Internally: 100ms smoothing, clamp to ±0.5dB for musical results
struct AutoGain {
    void reset(double fs)
    {
        // ~100ms exponential smoothing
        alpha = std::exp(-1.0 / (fs * 0.10));
        pSmooth = dSmooth = 0.0f;
    }

    float compute(float rmsProcessed, float rmsDry)
    {
        pSmooth = float(alpha) * pSmooth + float(1.0 - alpha) * rmsProcessed;
        dSmooth = float(alpha) * dSmooth + float(1.0 - alpha) * rmsDry;

        const float eps = 1e-7f;
        const float ratio = (pSmooth > eps ? dSmooth / pSmooth : 1.0f);

        // Clamp to ±0.5dB for musical makeup gain
        float deltaDb = 20.0f * std::log10(std::max(eps, ratio));
        deltaDb = std::clamp(deltaDb, -0.5f, 0.5f);
        return std::pow(10.0f, deltaDb / 20.0f);
    }

private:
    double alpha = 0.99;
    float  pSmooth = 0.0f, dSmooth = 0.0f;
};