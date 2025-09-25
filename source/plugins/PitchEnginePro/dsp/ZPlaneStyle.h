#pragma once
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_data_structures/juce_data_structures.h>
#include "BinaryData.h"

// Secret-sauce Z-plane macro. Loads LUT from BinaryData (embedded JSON),
// interpolates poles along a curated path, builds SOS cascade, applies filter.
class ZPlaneStyle {
public:
    void prepare (double fs) { fsHost = fs; buildFromEmbeddedLUT(); hasCoeffs = false;
                               // Initialize background filters
                               for (int k = 0; k < 8; ++k) {
                                   sosL_bg[k].prepare({fs, (juce::uint32)512, 1});
                                   sosR_bg[k].prepare({fs, (juce::uint32)512, 1});
                               } }

    // style in [0..1]
    void process (juce::AudioBuffer<float>& buf, float style);

    // Secret mode toggle
    void setSecretMode (bool on) { secret = on; }

private:
    struct Pole { double r=0.98; double thetaRef=0.0; };
    struct Step { float t=0; juce::Array<Pole> poles; };

    double fsHost = 48000.0;
    int numSections = 6; // 12-pole, 6 biquads
    juce::Array<Step> steps; // 33 steps

    // Working state with member flag (not static)
    bool hasCoeffs = false;
    juce::dsp::IIR::Filter<float> sosL[8], sosR[8];

    // Dual-path crossfading for smooth parameter changes (per DSP research)
    juce::dsp::IIR::Filter<float> sosL_bg[8], sosR_bg[8]; // background filters
    float crossfadeAmount = 0.0f; // 0=main, 1=background
    float prevMorphState = 0.0f;
    bool needsCrossfade = false;
    int crossfadeSamples = 0;
    static constexpr int kCrossfadeLength = 64; // samples
    static constexpr float kLargeJumpThreshold = 0.1f; // morph change triggering crossfade

    // Secret mode state
    bool secret = false;
    float morphState = 0.0f; // for slew
    juce::Random rng;

    // State-energy limiter (per DSP research)
    float outputEnergyL[8] = {0};
    float outputEnergyR[8] = {0};
    static constexpr float kEnergyDecay = 0.999f;
    static constexpr float kMaxEnergy = 24.0f; // ~+24dBFS internal headroom

    void buildFromEmbeddedLUT();
    void setCoefficientsFor (float tNorm, bool updateBackground = false);
};