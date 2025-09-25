#pragma once
#include <algorithm>
#include <array>
#include <complex>
#include <cmath>
#include <juce_dsp/juce_dsp.h>

// Include your authentic EMU tables - adjust path as needed
// extern const float AUTHENTIC_EMU_SHAPES[][12];
// extern const int   MORPH_PAIRS[][2];

// Include authentic EMU tables from the emu library
#if __has_include("../../emu/shapes/EMUAuthenticTables.h")
    #include "../../emu/shapes/EMUAuthenticTables.h"
#elif __has_include("shapes/EMUAuthenticTables.h")
    #include "shapes/EMUAuthenticTables.h"
#else
    #warning "Using stub EMU data - no audio output will be produced!"
    static const int AUTHENTIC_EMU_SAMPLE_RATE_REF = 48000;
    static const int AUTHENTIC_EMU_NUM_SHAPES = 2;
    static const int AUTHENTIC_EMU_NUM_PAIRS = 1;
    static const float AUTHENTIC_EMU_SHAPES[2][12] = {
        {0.95f,0.3f,0.93f,0.6f,0.9f,1.2f,0.88f,1.7f,0.85f,2.1f,0.83f,2.7f},
        {0.96f,0.35f,0.94f,0.65f,0.91f,1.25f,0.89f,1.75f,0.86f,2.15f,0.84f,2.75f}
    };
    static const int MORPH_PAIRS[1][2] = {{0, 1}};
    static const char* AUTHENTIC_EMU_IDS[1] = { "stub_pair" };
#endif

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
    void setLFOPhase(float radians);       // set phase (0..2p) for retrigger

    // Runtime model pack (optional): shapes as r,theta pairs per section (6 pairs => 12 floats per shape)
    void setModelPack(const float* shapes12, int numShapes, const int* pairs2, int numPairs);

    // Process IN-PLACE. Call this on the **WET ONLY** buffer.
    void process(juce::AudioBuffer<float>& buffer);

    struct BiquadSection {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
        float a1 = 0.0f, a2 = 0.0f;
        float z1 = 0.0f, z2 = 0.0f;

        static constexpr float denormEps = 1.0e-20f;

        inline void reset() { z1 = z2 = 0.0f; }

        static inline float flush(float v) { return (std::abs(v) < denormEps ? 0.0f : v); }

        inline float processSample(float x, float sat01) {
            float y = b0 * x + z1;
            if (! std::isfinite(y))
                y = 0.0f;

            const float nextZ1 = b1 * x - a1 * y + z2;
            const float nextZ2 = b2 * x - a2 * y;

            if (sat01 > 1.0e-6f) {
                const float drive = 1.0f + 3.0f * sat01;
                const float limited = std::clamp(y * drive, -8.0f, 8.0f);
                y = std::tanh(limited) / drive;
            }

            const float sanZ1 = std::isfinite(nextZ1) ? nextZ1 : 0.0f;
            const float sanZ2 = std::isfinite(nextZ2) ? nextZ2 : 0.0f;

            z1 = flush(sanZ1);
            z2 = flush(sanZ2);
            return flush(std::isfinite(y) ? y : 0.0f);
        }
    };

    struct PolePair { float r = 0.0f, theta = 0.0f; };

    void updateCoefficientsBlock(int blockSamples = 0);
    static void zpairToBiquad(const PolePair& p, float& a1, float& a2, float& b0, float& b1, float& b2);
    static void sanitiseSection(BiquadSection& section);

    static constexpr float minPoleRadius = 0.10f;
    static constexpr float maxPoleRadius = 0.995f;  // Restore EMU character - closer to unit circle
    static constexpr float stabilityMargin = 5.0e-4f;  // Tighter margin for more character

private:

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

    // Dynamic pack (if provided)
    bool useDynamicPack = false;
    int dynNumShapes = 0, dynNumPairs = 0;
    std::array<std::array<float,12>, 32> dynShapes{};
    std::array<std::array<int,2>, 32>   dynPairs{};

    friend struct AuthenticEMUZPlaneTestProbe;
};
