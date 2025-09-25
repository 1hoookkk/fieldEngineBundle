// Minimal, baked-in, modular DSP engine (hidden secret sauce)
#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>

namespace fe { namespace secretfx {

// Compact parameter snapshot (set once per block)
struct Snapshot {
    float morph01          = 0.4f;   // 0..1
    float intensity01      = 0.12f;  // 0..1 (kept subtle)
    float driveDb          = 0.0f;   // dB, -12..+12 typical
    float sectionSat01     = 0.08f;  // 0..1 per-section saturation
    bool  autoMakeup       = true;   // RMS-compensated

    // Light modulation baked in
    float lfoRateHz        = 0.35f;  // 0..8 Hz
    float lfoDepth01       = 0.05f;  // 0..1 morph modulation depth
};

// Engine is wet-only; host does mixing
class Engine {
public:
    Engine() = default;

    void prepare(double sampleRate, int samplesPerBlock, int numChannels) noexcept;
    void reset() noexcept;

    // Set a new per-block snapshot then process the wet buffer
    void setSnapshot(const Snapshot& s) noexcept { snap_ = s; }
    void setPairId(const juce::String& id) noexcept { defaultPair_ = id; }
    void process(juce::AudioBuffer<float>& wetBuffer) noexcept;

private:
    static constexpr int kSections = 6;

    struct Biquad {
        float b0=1, b1=0, b2=0, a1=0, a2=0;
        float z1=0, z2=0;
        inline void reset() noexcept { z1 = z2 = 0.0f; }
        inline float tick(float x, float satAmt) noexcept {
            float y = b0*x + z1;
            z1 = b1*x - a1*y + z2;
            z2 = b2*x - a2*y;
            if (satAmt > 1e-6f) {
                const float g = 1.0f + 4.0f*satAmt;
                // Cheap tanh-like soft clip
                y = juce::dsp::FastMathApproximations::tanh(y * g) / g;
            }
            return juce::isfinite(y) ? y : 0.0f;
        }
    };

    struct Pole { float r=0.95f, theta=0.0f; };

    // Runtime-loaded shapes from embedded JSON (fallback baked-in values used if load fails)
    struct PairShapes { std::array<Pole, kSections> A{}, B{}; };
    juce::HashMap<juce::String, PairShapes> pairs_;
    juce::String defaultPair_ { "vowel_pair" };

    // State
    float fs_ = 48000.0f;
    int   ch_ = 2;
    Snapshot snap_{};
    float lfoPhase_ = 0.0f;
    std::array<Pole, kSections> poles48_{}; // ref @ 48k style
    std::array<Biquad, kSections> L_{};
    std::array<Biquad, kSections> R_{};

    // RMS auto-makeup
    float preRMS_ = 1e-6f, postRMS_ = 1e-6f;
    juce::LinearSmoothedValue<float> makeup_;

    // Helpers
    static inline float wrapShortest(float a0, float a1) noexcept;
    static inline Pole interpPole(const Pole& a, const Pole& b, float t, float intensity01) noexcept;
    static inline void poleToBiquad(const Pole& p, Biquad& s) noexcept;
    void updateCoeffs(int numSamples) noexcept;

    void loadEmbeddedPairsIfNeeded() noexcept;
};

}} // namespace fe::secretfx
