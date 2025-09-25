#pragma once
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_data_structures/juce_data_structures.h>
// #include "BinaryData.h" // Remove binary data dependency for modular DSP library

// Secret-sauce Z-plane macro. Loads LUT from BinaryData (embedded JSON),
// interpolates poles along a curated path, builds SOS cascade, applies filter.
class ZPlaneStyle {
public:
    void prepare (double fs) { fsHost = fs; buildFromEmbeddedLUT(); hasCoeffs = false; }

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

    // Secret mode state
    bool secret = false;
    float morphState = 0.0f; // for slew
    juce::Random rng;

    void buildFromEmbeddedLUT();
    void setCoefficientsFor (float tNorm);
};