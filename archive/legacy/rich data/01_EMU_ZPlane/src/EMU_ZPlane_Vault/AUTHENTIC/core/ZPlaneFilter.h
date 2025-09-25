#pragma once
#include <array>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <numbers>
#include <juce_dsp/juce_dsp.h>

// Minimal self-contained Z-plane morphing filter engine
// - 6-section cascade (12th order)
// - Per-section saturation
// - RMS auto-makeup
// - Smoothed parameters (morph, intensity, drive)

namespace fe {

//==============================================================================
struct PolePair { float r = 0.95f; float theta = 0.0f; };
static constexpr int ZPLANE_N_SECTIONS = 6;

// Direct Form II Transposed biquad with optional saturation
class BiquadSection {
public:
    BiquadSection() { reset(); }

    void setCoeffs(float b0_, float b1_, float b2_, float a1_, float a2_) noexcept {
        b0 = b0_; b1 = b1_; b2 = b2_;
        a1 = a1_; a2 = a2_;
        if (!std::isfinite(z1)) z1 = 0.0f;
        if (!std::isfinite(z2)) z2 = 0.0f;
    }

    void enableSaturation(bool enable, float amount01 = 0.0f) noexcept {
        saturationEnabled = enable;
        saturationAmount = std::clamp(amount01, 0.0f, 1.0f);
    }

    void setSaturationAmount(float amount01) noexcept {
        saturationAmount = std::clamp(amount01, 0.0f, 1.0f);
    }

    inline float processSample(float x) noexcept {
        float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;

        if (saturationEnabled && saturationAmount > 0.0f) {
            const float gain = 1.0f + saturationAmount * 4.0f;
            y = juce::dsp::FastMathApproximations::tanh(y * gain);
        }

        if (!std::isfinite(y)) y = 0.0f;
        return y;
    }

    inline void reset() noexcept { z1 = z2 = 0.0f; }

private:
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
    float a1 = 0.0f, a2 = 0.0f;
    float z1 = 0.0f, z2 = 0.0f;
    bool saturationEnabled = true;
    float saturationAmount = 0.2f;
};

//==============================================================================
class ZPlaneFilter {
public:
    ZPlaneFilter() noexcept {
        // seed smoother defaults
        morphSmooth_.reset(44100.0, 0.02);
        morphSmooth_.setCurrentAndTargetValue(0.0f);
        intensitySmooth_.reset(44100.0, 0.02);
        intensitySmooth_.setCurrentAndTargetValue(0.4f);
        driveSmooth_.reset(44100.0, 0.01);
        driveSmooth_.setCurrentAndTargetValue(0.2f);
        makeupSmooth_.reset(44100.0, 0.05);
        makeupSmooth_.setCurrentAndTargetValue(1.0f);

        // Default shapes
        for (int i = 0; i < ZPLANE_N_SECTIONS; ++i) {
            shapeA_[i].r = 0.95f - i * 0.01f;
            shapeA_[i].theta = float((i + 1) * 0.15f);
            shapeB_[i].r = 0.88f - i * 0.005f;
            shapeB_[i].theta = float((i + 1) * 0.18f);
        }
    }

    void prepare(double sampleRate, int /*samplesPerBlock*/) noexcept {
        fs_ = static_cast<float>(sampleRate);
        morphSmooth_.reset(sampleRate, 0.02);
        intensitySmooth_.reset(sampleRate, 0.02);
        driveSmooth_.reset(sampleRate, 0.01);
        makeupSmooth_.reset(sampleRate, 0.05);
        morphSmooth_.setCurrentAndTargetValue(0.0f);
        intensitySmooth_.setCurrentAndTargetValue(0.4f);
        driveSmooth_.setCurrentAndTargetValue(0.2f);
        makeupSmooth_.setCurrentAndTargetValue(1.0f);
        preRMSsq_ = 1e-6f; postRMSsq_ = 1e-6f;
        for (auto& s : sectionsL_) s.reset();
        for (auto& s : sectionsR_) s.reset();
    }

    // Parameter setters
    void setDrive(float drive01) noexcept { driveSmooth_.setTargetValue(std::clamp(drive01, 0.0f, 1.0f)); }
    void setIntensity(float intensity01) noexcept { intensitySmooth_.setTargetValue(std::clamp(intensity01, 0.0f, 1.0f)); }
    void setMorph(float morph01) noexcept { morphSmooth_.setTargetValue(std::clamp(morph01, 0.0f, 1.0f)); }
    void setAutoMakeup(bool enabled) noexcept { autoMakeup_ = enabled; }
    void enableSectionSaturation(bool enabled) noexcept {
        sectionSaturationEnabled_ = enabled;
        for (auto& s : sectionsL_) s.enableSaturation(enabled, sectionSaturationAmount_);
        for (auto& s : sectionsR_) s.enableSaturation(enabled, sectionSaturationAmount_);
    }
    void setSectionSaturationAmount(float amount01) noexcept {
        sectionSaturationAmount_ = std::clamp(amount01, 0.0f, 1.0f);
        for (auto& s : sectionsL_) s.setSaturationAmount(sectionSaturationAmount_);
        for (auto& s : sectionsR_) s.setSaturationAmount(sectionSaturationAmount_);
    }

    // Shapes
    void setShapeA(const std::array<PolePair, ZPLANE_N_SECTIONS>& s) noexcept { shapeA_ = s; }
    void setShapeB(const std::array<PolePair, ZPLANE_N_SECTIONS>& s) noexcept { shapeB_ = s; }

    // Processing API
    void updateCoefficientsBlock() noexcept {
        lastMorph_ = morphSmooth_.getNextValue();
        lastIntensity_ = intensitySmooth_.getNextValue();
        lastDrive_ = driveSmooth_.getNextValue();

        const float intensityBoost = 1.0f + lastIntensity_ * 0.06f;

        for (int i = 0; i < ZPLANE_N_SECTIONS; ++i) {
            PolePair p = interpPole(shapeA_[i], shapeB_[i], lastMorph_);
            p.r = std::min(p.r * intensityBoost, 0.999999f);

            float a1, a2;
            polePairToDenCoeffs(p, a1, a2);

            const float rz = std::clamp(0.9f * p.r, 0.0f, 0.999f);
            const float c = juce::dsp::FastMathApproximations::cos(p.theta);
            float b0 = 1.0f;
            float b1 = -2.0f * rz * c;
            float b2 = rz * rz;

            const float norm = 1.0f / std::max(0.25f, std::abs(b0) + std::abs(b1) + std::abs(b2));
            b0 *= norm; b1 *= norm; b2 *= norm;

            sectionsL_[i].setCoeffs(b0, b1, b2, a1, a2);
            sectionsR_[i].setCoeffs(b0, b1, b2, a1, a2);
            sectionsL_[i].enableSaturation(sectionSaturationEnabled_, sectionSaturationAmount_);
            sectionsR_[i].enableSaturation(sectionSaturationEnabled_, sectionSaturationAmount_);
        }
    }

    void processBlock(float* left, float* right, int numSamples) noexcept {
        if (!left || !right || numSamples <= 0) return;
        for (int n = 0; n < numSamples; ++n) {
            const float l = left[n];
            const float r = right[n];
            const float outL = processSampleCh(l, sectionsL_);
            const float outR = processSampleCh(r, sectionsR_);
            left[n] = outL; right[n] = outR;
        }
    }

    void reset() noexcept {
        for (auto& s : sectionsL_) s.reset();
        for (auto& s : sectionsR_) s.reset();
        preRMSsq_ = 1e-6f; postRMSsq_ = 1e-6f;
        makeupSmooth_.setCurrentAndTargetValue(1.0f);
    }

private:
    static inline PolePair interpPole(const PolePair& p0, const PolePair& p1, float t) noexcept {
        PolePair out;
        out.r = p0.r + t * (p1.r - p0.r);
        float a0 = p0.theta, a1 = p1.theta;
        const float pi = std::numbers::pi_v<float>;
        float diff = std::fmod(a1 - a0 + pi, 2.0f * pi) - pi;
        out.theta = a0 + diff * t;
        out.r = std::min(out.r, 0.999999f);
        if (!std::isfinite(out.r)) out.r = 0.95f;
        if (!std::isfinite(out.theta)) out.theta = 0.0f;
        return out;
    }

    static inline void polePairToDenCoeffs(const PolePair& p, float& outA1, float& outA2) noexcept {
        outA1 = -2.0f * p.r * juce::dsp::FastMathApproximations::cos(p.theta);
        outA2 = p.r * p.r;
        if (!std::isfinite(outA1)) outA1 = 0.0f;
        if (!std::isfinite(outA2)) outA2 = 0.0f;
    }

    inline void updateRMS(float x, float& state) noexcept {
        const float tau = 0.1f;
        const float alpha = 1.0f - std::exp(-1.0f / (tau * fs_));
        const float x2 = x * x;
        state = (1.0f - alpha) * state + alpha * x2;
        if (state < 1e-12f) state = 1e-12f;
    }

    inline float processSampleCh(float input, std::array<BiquadSection, ZPLANE_N_SECTIONS>& sections) noexcept {
        const float pre = input * (1.0f + lastDrive_ * 3.0f);
        updateRMS(pre, preRMSsq_);
        float x = pre;
        for (int i = 0; i < ZPLANE_N_SECTIONS; ++i) x = sections[i].processSample(x);
        updateRMS(x, postRMSsq_);
        float out = x;
        if (autoMakeup_) {
            const float preRMS = std::sqrt(std::max(preRMSsq_, 1e-12f));
            const float postRMS = std::sqrt(std::max(postRMSsq_, 1e-12f));
            float correction = 1.0f;
            if (postRMS > 1e-9f) correction = preRMS / postRMS;
            correction = std::clamp(correction, 0.5f, 2.0f);
            makeupSmooth_.setTargetValue(correction);
            const float smoothed = makeupSmooth_.getNextValue();
            out *= smoothed;
        }
        if (!std::isfinite(out)) out = 0.0f;
        return out;
    }

    std::array<PolePair, ZPLANE_N_SECTIONS> shapeA_{};
    std::array<PolePair, ZPLANE_N_SECTIONS> shapeB_{};
    std::array<BiquadSection, ZPLANE_N_SECTIONS> sectionsL_{};
    std::array<BiquadSection, ZPLANE_N_SECTIONS> sectionsR_{};

    juce::LinearSmoothedValue<float> morphSmooth_;
    juce::LinearSmoothedValue<float> intensitySmooth_;
    juce::LinearSmoothedValue<float> driveSmooth_;
    juce::LinearSmoothedValue<float> makeupSmooth_;

    float preRMSsq_ = 1e-6f;
    float postRMSsq_ = 1e-6f;

    bool autoMakeup_ = true;
    bool sectionSaturationEnabled_ = true;
    float sectionSaturationAmount_ = 0.2f;

    float fs_ = 48000.0f;

    float lastMorph_ = 0.0f;
    float lastIntensity_ = 0.4f;
    float lastDrive_ = 0.2f;
};

} // namespace fe
