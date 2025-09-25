#pragma once

#include <vector>
#include <array>
#include <atomic>
#include <memory>
#include <cmath>
#include <complex>
#include <algorithm>
#include "../preset_loader_json.hpp"
#include "ZPlaneHelpers.h"
#include "ZPlaneTables_T1.h"
#include "ZPlaneTables_T2.h"
#include "../preset/A2KRuntime.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace fe {

//==============================================================================
// Simple parameter smoothing (JUCE-free version)
template<typename T>
class SmoothedValue {
public:
    SmoothedValue() = default;

    void reset(double sampleRate, double rampTimeSeconds) {
        sampleRate_ = sampleRate;
        rampLength_ = static_cast<int>(rampTimeSeconds * sampleRate);
        stepsToTarget_ = 0;
        currentValue_ = targetValue_ = T{};
    }

    void setTargetValue(T newTarget) {
        if (newTarget != targetValue_) {
            targetValue_ = newTarget;
            stepsToTarget_ = rampLength_;
            if (stepsToTarget_ > 0) {
                step_ = (targetValue_ - currentValue_) / T(stepsToTarget_);
            } else {
                currentValue_ = targetValue_;
            }
        }
    }

    void setCurrentAndTargetValue(T value) {
        currentValue_ = targetValue_ = value;
        stepsToTarget_ = 0;
    }

    T getNextValue() {
        if (stepsToTarget_ > 0) {
            currentValue_ += step_;
            --stepsToTarget_;
            if (stepsToTarget_ == 0) {
                currentValue_ = targetValue_;
            }
        }
        return currentValue_;
    }

private:
    T currentValue_{}, targetValue_{}, step_{};
    double sampleRate_ = 44100.0;
    int rampLength_ = 0;
    int stepsToTarget_ = 0;
};

//==============================================================================
// PolePair structure for Z-plane morphing
struct PolePair {
    float r = 0.95f;      // radius (0..1, clamp < 1)
    float theta = 0.0f;   // angle in radians
};

// Configurable number of biquad sections (6 => 12th order Audity flavor)
static constexpr int ZPLANE_N_SECTIONS = 6;

//==============================================================================
// Enhanced BiquadSection with per-section saturation (Direct Form II Transposed)
class BiquadSection {
public:
    BiquadSection() noexcept { reset(); }

    // RT-safe coefficient setter (call at block-rate or before processing)
    void setCoeffs(float b0_, float b1_, float b2_, float a1_, float a2_) noexcept {
        // store coefficients (a0 assumed 1)
        b0 = b0_; b1 = b1_; b2 = b2_;
        a1 = a1_; a2 = a2_;
        // ensure states are finite
        if (!std::isfinite(z1)) z1 = 0.0f;
        if (!std::isfinite(z2)) z2 = 0.0f;
    }

    // enable/disable section saturation and set amount (0..1)
    void enableSaturation(bool enable, float amount01 = 0.0f) noexcept {
        saturationEnabled = enable;
        saturationAmount = std::clamp(amount01, 0.0f, 1.0f);
    }

    void setSaturationAmount(float amount01) noexcept {
        saturationAmount = std::clamp(amount01, 0.0f, 1.0f);
    }

    inline float processSample(float x) noexcept {
        // DF2T
        float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;

        // optional per-section saturation
        if (saturationEnabled && saturationAmount > 0.0f) {
            // map amount to k (0..2)
            float k = saturationAmount * 2.0f;
            // tanh saturation for musical character
            y = std::tanh(k * y);
        }

        // guard NaNs/Infs
        if (!std::isfinite(y)) y = 0.0f;

        return y;
    }

    inline void reset() noexcept { z1 = z2 = 0.0f; }

    // Legacy compatibility method
    inline float process(float x) noexcept { return processSample(x); }

    // Legacy setLowpass method for backward compatibility
    void setLowpass(float freq, float q, float sampleRate) {
        double db0, db1, db2, da1, da2;
        ZPlaneHelpers::calculateLowpassCoeffs((double)freq, (double)q, (double)sampleRate,
                                            db0, db1, db2, da1, da2);
        setCoeffs((float)db0, (float)db1, (float)db2, (float)da1, (float)da2);

        // Apply stability guards
        if (std::abs(a2) >= 1.0f) {
            a2 = std::copysign(0.999f, a2);
        }
        if (std::abs(a1) >= 2.0f) {
            a1 = std::copysign(1.999f, a1);
        }
    }

    void setCoefficients(float new_b0, float new_b1, float new_b2, float new_a1, float new_a2) {
        setCoeffs(new_b0, new_b1, new_b2, new_a1, new_a2);
    }

private:
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
    float a1 = 0.0f, a2 = 0.0f;
    float z1 = 0.0f, z2 = 0.0f;

    bool saturationEnabled = false;
    float saturationAmount = 0.0f;
};

//==============================================================================
// Z-Plane Filter with authentic pole/zero morphing (Audity 2000 style)
class ZPlaneFilter {
public:
    ZPlaneFilter() noexcept;
    ~ZPlaneFilter() noexcept = default;

    // Real-time safe prepare (call in prepareToPlay)
    void prepare(double sampleRate, int samplesPerBlock) noexcept;

    // Block-rate update: compute coefficients from shapes + morph + intensity
    // Call at the start of each audio block (before processing samples)
    void updateCoefficientsBlock() noexcept;

    // Per-sample processing (mono). For stereo, call per-channel or duplicate instance.
    float processSample(float input) noexcept;

    // Per-sample processing with separate channel state
    float processSampleCh(float input, std::array<BiquadSection, ZPLANE_N_SECTIONS>& sections) noexcept;

    // Convenience - process interleaved stereo block (real-time safe)
    // Call after updateCoefficientsBlock
    void processBlock(float* left, float* right, int numSamples) noexcept;

    // Parameter setters (real-time safe)
    void setDrive(float drive01) noexcept;       // 0..1 -> pre-drive gain
    void setIntensity(float intensity01) noexcept; // 0..1 -> resonance scaling
    void setMorph(float morph01) noexcept;       // 0..1 -> morph between shapeA/B
    void setAutoMakeup(bool enabled) noexcept;
    void enableSectionSaturation(bool enabled) noexcept;
    void setSectionSaturationAmount(float amount01) noexcept;

    // Load user shapes (N sections). Must be called from UI/main thread (not audio)
    void setShapeA(const std::array<PolePair, ZPLANE_N_SECTIONS>& s) noexcept;
    void setShapeB(const std::array<PolePair, ZPLANE_N_SECTIONS>& s) noexcept;

    // Legacy compatibility methods
    void setCutoff(float cutoff01) noexcept { setMorph(cutoff01); }
    void setResonance(float res01) noexcept { setIntensity(res01); }
    void setMorphTargets(float t1, float t2) noexcept { setMorph((t1 + t2) * 0.5f); }
    void setFilterModel(int modelId) noexcept { modelId_ = modelId; }

    // Reset internal states
    void reset() noexcept;

private:
    // helpers
    static inline PolePair interpPole(const PolePair& p0, const PolePair& p1, float t) noexcept;
    static inline void polePairToDenCoeffs(const PolePair& p, float& outA1, float& outA2) noexcept;
    inline void updateRMS(float x, float& state) noexcept; // stores squared RMS
    inline float safeSqrt(float s) noexcept { return std::sqrt(std::max(s, 1e-12f)); }

    // members
    std::array<PolePair, ZPLANE_N_SECTIONS> shapeA_;
    std::array<PolePair, ZPLANE_N_SECTIONS> shapeB_;
    std::array<BiquadSection, ZPLANE_N_SECTIONS> sectionsL_;
    std::array<BiquadSection, ZPLANE_N_SECTIONS> sectionsR_;

    // Smoothed parameters (custom SmoothedValue)
    SmoothedValue<float> morphSmooth_;     // 20ms default
    SmoothedValue<float> intensitySmooth_; // 20ms default
    SmoothedValue<float> driveSmooth_;     // 10ms default
    SmoothedValue<float> makeupSmooth_;    // smoothing for makeup gain

    // RMS states store energy (x^2 running average)
    float preRMSsq_ = 1e-6f;
    float postRMSsq_ = 1e-6f;

    // runtime flags
    bool autoMakeup_ = true;
    bool sectionSaturationEnabled_ = true;

    // saturation amount (0..1)
    float sectionSaturationAmount_ = 0.2f;

    // sample rate
    float fs_ = 48000.0f;

    // temporary block values (updated in updateCoefficientsBlock)
    float lastMorph_ = 0.0f;
    float lastIntensity_ = 0.4f;
    float lastDrive_ = 0.2f;

    // Legacy compatibility
    int modelId_ = 1012;
};

//==============================================================================
// ZPlaneFilter Implementation (inline for header-only approach)
//==============================================================================

inline ZPlaneFilter::ZPlaneFilter() noexcept {
    // initialize SmoothedValue defaults (these will be reset in prepare)
    morphSmooth_.reset(44100.0, 0.02);
    morphSmooth_.setCurrentAndTargetValue(0.0f);

    intensitySmooth_.reset(44100.0, 0.02);
    intensitySmooth_.setCurrentAndTargetValue(0.4f);

    driveSmooth_.reset(44100.0, 0.01);
    driveSmooth_.setCurrentAndTargetValue(0.2f);

    makeupSmooth_.reset(44100.0, 0.05);
    makeupSmooth_.setCurrentAndTargetValue(1.0f);

    // default simple shapes (small placeholders — replace with actual presets)
    for (int i = 0; i < ZPLANE_N_SECTIONS; ++i) {
        // Slightly spaced resonances
        shapeA_[i].r = 0.95f - i * 0.01f;
        shapeA_[i].theta = float( (i + 1) * 0.15f );
        shapeB_[i].r = 0.88f - i * 0.005f;
        shapeB_[i].theta = float( (i + 1) * 0.18f );
    }
}

inline void ZPlaneFilter::prepare(double sampleRate, int /*samplesPerBlock*/) noexcept {
    fs_ = static_cast<float>(sampleRate);

    // reset smoothing with correct sample rate and ramp times
    morphSmooth_.reset(sampleRate, 0.02);    // 20ms for morph/intensity
    intensitySmooth_.reset(sampleRate, 0.02);
    driveSmooth_.reset(sampleRate, 0.01);    // 10ms for drive
    makeupSmooth_.reset(sampleRate, 0.05);   // 50ms smoothing for makeup gain

    // seed smoothed values
    morphSmooth_.setCurrentAndTargetValue(0.0f);
    intensitySmooth_.setCurrentAndTargetValue(0.4f);
    driveSmooth_.setCurrentAndTargetValue(0.2f);
    makeupSmooth_.setCurrentAndTargetValue(1.0f);

    preRMSsq_ = 1e-6f;
    postRMSsq_ = 1e-6f;

    // reset internal sections
    for (auto& s : sectionsL_) s.reset();
    for (auto& s : sectionsR_) s.reset();
}

inline void ZPlaneFilter::setDrive(float drive01) noexcept {
    driveSmooth_.setTargetValue(std::clamp(drive01, 0.0f, 1.0f));
}

inline void ZPlaneFilter::setIntensity(float intensity01) noexcept {
    intensitySmooth_.setTargetValue(std::clamp(intensity01, 0.0f, 1.0f));
}

inline void ZPlaneFilter::setMorph(float morph01) noexcept {
    morphSmooth_.setTargetValue(std::clamp(morph01, 0.0f, 1.0f));
}

inline void ZPlaneFilter::setAutoMakeup(bool enabled) noexcept {
    autoMakeup_ = enabled;
}

inline void ZPlaneFilter::enableSectionSaturation(bool enabled) noexcept {
    sectionSaturationEnabled_ = enabled;
    for (auto& s : sectionsL_) s.enableSaturation(enabled, sectionSaturationAmount_);
    for (auto& s : sectionsR_) s.enableSaturation(enabled, sectionSaturationAmount_);
}

inline void ZPlaneFilter::setSectionSaturationAmount(float amount01) noexcept {
    sectionSaturationAmount_ = std::clamp(amount01, 0.0f, 1.0f);
    for (auto& s : sectionsL_) s.setSaturationAmount(sectionSaturationAmount_);
    for (auto& s : sectionsR_) s.setSaturationAmount(sectionSaturationAmount_);
}

inline void ZPlaneFilter::setShapeA(const std::array<PolePair, ZPLANE_N_SECTIONS>& s) noexcept {
    shapeA_ = s;
}

inline void ZPlaneFilter::setShapeB(const std::array<PolePair, ZPLANE_N_SECTIONS>& s) noexcept {
    shapeB_ = s;
}

inline void ZPlaneFilter::reset() noexcept {
    for (auto& s : sectionsL_) s.reset();
    for (auto& s : sectionsR_) s.reset();
    preRMSsq_ = 1e-6f;
    postRMSsq_ = 1e-6f;
    makeupSmooth_.setCurrentAndTargetValue(1.0f);
}

inline PolePair ZPlaneFilter::interpPole(const PolePair& p0, const PolePair& p1, float t) noexcept {
    PolePair out;
    out.r = p0.r + t * (p1.r - p0.r); // linear interpolation on radius
    // shortest wrap interpolation for angles
    float a0 = p0.theta, a1 = p1.theta;
    float diff = std::fmodf(a1 - a0 + M_PI, 2.0f * M_PI) - M_PI; // wrap to [-pi, pi]
    out.theta = a0 + diff * t;
    // clamp radius for stability
    out.r = std::min(out.r, 0.999999f);
    if (!std::isfinite(out.r)) out.r = 0.95f;
    if (!std::isfinite(out.theta)) out.theta = 0.0f;
    return out;
}

inline void ZPlaneFilter::polePairToDenCoeffs(const PolePair& p, float& outA1, float& outA2) noexcept {
    // Denominator: 1 - 2 r cos(theta) z^-1 + r^2 z^-2
    outA1 = -2.0f * p.r * std::cos(p.theta);
    outA2 = p.r * p.r;
    if (!std::isfinite(outA1)) outA1 = 0.0f;
    if (!std::isfinite(outA2)) outA2 = 0.0f;
}

inline void ZPlaneFilter::updateCoefficientsBlock() noexcept {
    // fetch smoothed block-start values
    lastMorph_ = morphSmooth_.getNextValue();
    lastIntensity_ = intensitySmooth_.getNextValue();
    lastDrive_ = driveSmooth_.getNextValue();

    // Map intensity -> subtle radius boost (musical) and possible Q-like behavior
    // intensity scaling (1.0 -> 1.06 multiplier on r maximum, clamped)
    float intensityBoost = 1.0f + lastIntensity_ * 0.06f; // jmap(lastIntensity_, 0.0f, 1.0f, 1.0f, 1.06f)

    for (int i = 0; i < ZPLANE_N_SECTIONS; ++i) {
        // interpolate pole pair
        PolePair p = interpPole(shapeA_[i], shapeB_[i], lastMorph_);

        // apply intensity: slightly increase r (increases resonance)
        p.r = std::min(p.r * intensityBoost, 0.999999f);

        // Derive denominator coeffs
        float a1, a2;
        polePairToDenCoeffs(p, a1, a2);

        // Zero-pair numerator: create matching zeros at slightly smaller radius for stability
        const float rz = std::clamp(0.9f * p.r, 0.0f, 0.999f);
        const float c = std::cos(p.theta);
        float b0 = 1.0f;
        float b1 = -2.0f * rz * c;
        float b2 = rz * rz;

        // Light normalization to prevent runaway gain
        const float norm = 1.0f / std::max(0.25f, std::abs(b0) + std::abs(b1) + std::abs(b2));
        b0 *= norm;
        b1 *= norm;
        b2 *= norm;

        sectionsL_[i].setCoeffs(b0, b1, b2, a1, a2);
        sectionsR_[i].setCoeffs(b0, b1, b2, a1, a2);
        // update saturation enable and amount in case toggled
        sectionsL_[i].enableSaturation(sectionSaturationEnabled_, sectionSaturationAmount_);
        sectionsR_[i].enableSaturation(sectionSaturationEnabled_, sectionSaturationAmount_);
    }
}

inline void ZPlaneFilter::updateRMS(float x, float& state) noexcept {
    // Leaky integrator for squared RMS (tau = 0.1s)
    float tau = 0.1f;
    float alpha = 1.0f - std::expf(-1.0f / (tau * fs_));
    float x2 = x * x;
    state = (1.0f - alpha) * state + alpha * x2;
    // keep floor to avoid div by zero
    if (state < 1e-12f) state = 1e-12f;
}

inline float ZPlaneFilter::processSampleCh(float input, std::array<BiquadSection, ZPLANE_N_SECTIONS>& sections) noexcept {
    // pre-drive: map drive 0..1 to (1 + drive * 3)
    float pre = input * (1.0f + lastDrive_ * 3.0f);

    // track pre RMS (squared)
    updateRMS(pre, preRMSsq_);

    float x = pre;
    // pass through sections
    for (int i = 0; i < ZPLANE_N_SECTIONS; ++i) {
        x = sections[i].processSample(x);
    }

    // track post RMS
    updateRMS(x, postRMSsq_);

    float out = x;
    if (autoMakeup_) {
        // compute ratio of RMS magnitudes (sqrt of stored sq values)
        float preRMS = safeSqrt(preRMSsq_);
        float postRMS = safeSqrt(postRMSsq_);
        float correction = 1.0f;
        if (postRMS > 1e-9f) correction = preRMS / postRMS;
        correction = std::clamp(correction, 0.5f, 2.0f);

        // smooth the correction (target)
        makeupSmooth_.setTargetValue(correction);
        float smoothedCorr = makeupSmooth_.getNextValue();

        out = out * smoothedCorr;
    }

    // final guard
    if (!std::isfinite(out)) out = 0.0f;

    return out;
}

inline float ZPlaneFilter::processSample(float input) noexcept {
    // Legacy mono compatibility - uses left channel
    return processSampleCh(input, sectionsL_);
}

// convenience stereo block processing
inline void ZPlaneFilter::processBlock(float* left, float* right, int numSamples) noexcept {
    for (int n = 0; n < numSamples; ++n) {
        float l = left[n];
        float r = right[n];

        // process each sample with separate channel state
        float outL = processSampleCh(l, sectionsL_);
        float outR = processSampleCh(r, sectionsR_);

        left[n] = outL;
        right[n] = outR;
    }
}

//==============================================================================
// Simple LFO for modulation
class LFO {
public:
    LFO() = default;

    void prepare(double sampleRate) {
        sampleRate_ = sampleRate;
        phase_ = 0.0f;
    }

    void setFrequency(float hz) {
        phaseIncrement_ = hz / sampleRate_;
    }

    void setShape(const std::string& shape) {
        shape_ = shape;
    }

    float getNextSample() {
        float output = 0.0f;

        if (shape_ == "sine") {
            output = std::sin(2.0f * M_PI * phase_);
        } else if (shape_ == "triangle") {
            output = 4.0f * std::abs(phase_ - 0.5f) - 1.0f;
        } else if (shape_ == "square") {
            output = phase_ < 0.5f ? -1.0f : 1.0f;
        } else {
            output = 2.0f * phase_ - 1.0f; // sawtooth
        }

        phase_ += phaseIncrement_;
        if (phase_ >= 1.0f) phase_ -= 1.0f;

        return output;
    }

private:
    double sampleRate_ = 44100.0;
    float phase_ = 0.0f;
    float phaseIncrement_ = 0.0f;
    std::string shape_ = "sine";
};

//==============================================================================
// Main DSP Engine
class DSPEngine {
public:
    DSPEngine() = default;

    void prepare(double sampleRate, int blockSize, int numChannels) {
        sampleRate_ = sampleRate;
        blockSize_ = blockSize;
        numChannels_ = numChannels;

        filter_.prepare(sampleRate, blockSize);

        for (auto& lfo : lfos_) {
            lfo.prepare(sampleRate);
        }
    }

    // Z-Plane Filter parameter setters (new interface)
    void setDrive(float drive01) { filter_.setDrive(drive01); }
    void setIntensity(float intensity01) { filter_.setIntensity(intensity01); }
    void setMorph(float morph01) { filter_.setMorph(morph01); }
    void setAutoMakeup(bool enabled) { filter_.setAutoMakeup(enabled); }
    void enableSectionSaturation(bool enabled) { filter_.enableSectionSaturation(enabled); }
    void setSectionSaturationAmount(float amount01) { filter_.setSectionSaturationAmount(amount01); }
    void setShapeA(const std::array<PolePair, ZPLANE_N_SECTIONS>& s) { filter_.setShapeA(s); }
    void setShapeB(const std::array<PolePair, ZPLANE_N_SECTIONS>& s) { filter_.setShapeB(s); }

    // Legacy compatibility methods
    void setFilter(int modelId, float cutoff01, float res01) {
        filter_.setFilterModel(modelId);
        filter_.setCutoff(cutoff01);
        filter_.setResonance(res01);
    }

    void setMorphTargets(float t1, float t2) {
        filter_.setMorphTargets(t1, t2);
    }

    void setCrossfadeMs(float ms) {
        // TODO: Implement crossfade for preset changes
        crossfadeMs_ = ms;
    }

    void setHostTempoBpm(double bpm) {
        hostBpm_.store(bpm);
    }

    // ---- Audity 2000 Integration ----
    bool loadAudityBank(const juce::String& bankPath = "extracted_xtreme") {
        juce::File bankDir(bankPath);
        auto result = a2k::loadBank(bankDir, audityBank_);

        if (result.wasOk() && audityBank_.presets.size() >= 2) {
            // Set up default preset pair for morphing
            currentPresetA_ = 0;
            currentPresetB_ = 1;
            updateAudityFilter();
            return true;
        }
        return false;
    }

    void setAudityPresets(int presetA, int presetB) {
        if (presetA >= 0 && presetA < audityBank_.presets.size() &&
            presetB >= 0 && presetB < audityBank_.presets.size()) {
            currentPresetA_ = presetA;
            currentPresetB_ = presetB;
            updateAudityFilter();
        }
    }

    int getNumAudityPresets() const { return audityBank_.presets.size(); }

    juce::String getAudityPresetName(int index) const {
        if (index >= 0 && index < audityBank_.presets.size()) {
            return audityBank_.presets[index].name;
        }
        return "";
    }

    void ensureLFO(const std::string& id, float hz, const std::string& shape) {
        // Find or create LFO
        if (lfoCount_ < MAX_LFOS) {
            auto& lfo = lfos_[lfoCount_];
            lfo.prepare(sampleRate_);
            lfo.setFrequency(hz);
            lfo.setShape(shape);
            lfoCount_++;
        }
    }

    void processBlock(float* leftChannel, float* rightChannel, int numSamples) {
        // Control-rate LFO processing: update once per block for RT-safety
        for (int i = 0; i < lfoCount_ && i < MAX_LFOS; ++i) {
            // Compute block-averaged LFO value for modulation
            // In the future, this would be applied to filter parameters
            lfoValues_[i] = lfos_[i].getNextSample();

            // Advance LFO phase for the entire block to maintain timing accuracy
            const int samplesToAdvance = numSamples - 1; // -1 because we already got one sample
            for (int advance = 0; advance < samplesToAdvance; ++advance) {
                lfos_[i].getNextSample(); // Maintain LFO phase accuracy
            }
        }

        // Main filter processing with block-rate coefficient updates
        filter_.updateCoefficientsBlock();
        filter_.processBlock(leftChannel, rightChannel, numSamples);
    }

private:
    static constexpr int MAX_LFOS = 8;

    ZPlaneFilter filter_;
    std::array<LFO, MAX_LFOS> lfos_;
    std::array<float, MAX_LFOS> lfoValues_; // Control-rate LFO values
    int lfoCount_ = 0;

    double sampleRate_ = 44100.0;
    int blockSize_ = 512;
    int numChannels_ = 2;
    float crossfadeMs_ = 15.0f;

    std::atomic<double> hostBpm_{120.0};

    // ---- Audity 2000 Bank Data ----
    a2k::BankData audityBank_;
    int currentPresetA_ = 0;
    int currentPresetB_ = 1;

    // Helper to convert Audity poles to ZPlane sections
    void updateAudityFilter() {
        if (audityBank_.presets.isEmpty()) return;

        const auto& presetA = audityBank_.presets[currentPresetA_];
        const auto& presetB = audityBank_.presets[currentPresetB_];

        // Convert Audity sections to ZPlane format
        std::array<PolePair, ZPLANE_N_SECTIONS> shapeA, shapeB;

        for (int i = 0; i < ZPLANE_N_SECTIONS; ++i) {
            if (i < presetA.sections.size()) {
                // Convert a2k::PolePair to fe::PolePair
                const auto& audityPole = presetA.sections[i].poles;
                shapeA[i] = {audityPole.r, audityPole.theta};
            } else {
                shapeA[i] = {0.8f, 0.0f}; // Default pole
            }

            if (i < presetB.sections.size()) {
                // Convert a2k::PolePair to fe::PolePair
                const auto& audityPole = presetB.sections[i].poles;
                shapeB[i] = {audityPole.r, audityPole.theta};
            } else {
                shapeB[i] = {0.8f, 0.0f}; // Default pole
            }
        }

        filter_.setShapeA(shapeA);
        filter_.setShapeB(shapeB);
    }
};

} // namespace fe