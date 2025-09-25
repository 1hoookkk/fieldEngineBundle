#pragma once
#include <array>
#include <complex>
#include <cmath>
#include <juce_dsp/juce_dsp.h>

// Include your authentic EMU tables - adjust path as needed
// extern const float AUTHENTIC_EMU_SHAPES[][12];
// extern const int   MORPH_PAIRS[][2];

// Temporary shim for compilation - replace with your real tables
static const float AUTHENTIC_EMU_SHAPES[2][12] = {
    {0.95f,0.3f, 0.93f,0.6f, 0.9f,1.2f, 0.88f,1.7f, 0.85f,2.1f, 0.83f,2.7f},
    {0.96f,0.35f,0.94f,0.65f,0.91f,1.25f,0.89f,1.75f,0.86f,2.15f,0.84f,2.75f}
};
static const int MORPH_PAIRS[1][2] = {{0,1}};

/**
 * AUTHENTIC EMU Z-Plane Morphing Filter
 * Production-ready engine with neutral defaults, early-exit optimization,
 * sample-rate remapping, and wet-only processing design.
 */
class AuthenticEMUZPlane
{
public:
    using MorphPair = int; // index into MORPH_PAIRS

    AuthenticEMUZPlane();

    void prepareToPlay (double sampleRate);
    void reset();

    // Parameters (set at block start or from message thread)
    void setMorphPair(MorphPair p);
    void setMorphPosition(float pos01);    // [0..1]
    void setIntensity(float amt01);        // [0..1] - 0 = neutral/transparent
    void setDrive(float driveDb);          // dB - 0 = unity gain
    void setSectionSaturation(float amt01);// [0..1] - 0 = clean
    void setAutoMakeup(bool enabled);      // compensate for intensity changes
    void setLFORate(float hz);             // 0.02..8 Hz
    void setLFODepth(float depth01);       // [0..1]

    // Process IN-PLACE. Call this on the **WET ONLY** buffer.
    void process(juce::AudioBuffer<float>& buffer);

private:
    struct BiquadSection {
        float b0=1, b1=0, b2=0, a1=0, a2=0;
        float z1=0, z2=0;
        inline void reset() { z1=z2=0; }
        inline float processSample(float x, float sat01) {
            float y = b0*x + z1;
            z1 = b1*x - a1*y + z2;
            z2 = b2*x - a2*y;
            if (sat01 > 1e-6f) {
                const float d = 1.0f + 3.0f*sat01;
                y = std::tanh(y*d) / d;
            }
            return y;
        }
    };

    struct PolePair { float r, theta; };

    void updateCoefficientsBlock();
    static void zpairToBiquad(const PolePair& p, float& a1, float& a2, float& b0, float& b1, float& b2);

    // --- State ---
    float fs = 48000.0f;
    MorphPair currentPair = 0;
    float currentMorph = 0.5f;
    float currentIntensity = 0.0f; // neutral default (null-friendly)
    float driveLin = 1.0f;         // neutral default (0dB)
    float sectionSaturation = 0.0f; // clean default
    bool  autoMakeup = false;

    float lfoRate = 0.0f, lfoDepth = 0.0f, lfoPhase = 0.0f;

    juce::LinearSmoothedValue<float> morphSm, intenSm;
    BiquadSection sectionsL[6], sectionsR[6];

    PolePair polesRef48[6]; // interpolated @48k reference
    PolePair polesFs[6];    // remapped to current fs
};