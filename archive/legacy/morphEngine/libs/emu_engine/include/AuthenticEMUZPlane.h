#pragma once
#include <array>
#include <complex>
#include <cmath>
#include <juce_dsp/juce_dsp.h>

// Include your authentic EMU tables - adjust path as needed
// extern const float AUTHENTIC_EMU_SHAPES[][12];
// extern const int   MORPH_PAIRS[][2];

// Curated authentic shapes (48k reference): (r, theta) x 6 sections
// Pairs: 0=vowel_pair (Ae→Oo), 1=bell_pair (Bell→MetallicCluster), 2=low_pair (Low→FormantPad)
static const float AUTHENTIC_EMU_SHAPES[6][12] = {
    // 0: vowel_pair A (Vowel_Ae)
    {0.95f,0.0104719755f, 0.96f,0.0196349541f, 0.985f,0.0392699082f, 0.992f,0.1178097245f, 0.993f,0.3272492349f, 0.985f,0.4581489288f},
    // 1: bell_pair A (Bell_Metallic)
    {0.996f,0.1439896633f, 0.995f,0.1832595715f, 0.994f,0.2879793267f, 0.993f,0.3926990818f, 0.992f,0.5497787144f, 0.990f,0.7853981636f},
    // 2: low_pair A (Low_LP_Punch)
    {0.88f,0.0039269908f, 0.90f,0.0078539816f, 0.92f,0.0157079633f, 0.94f,0.0327249235f, 0.96f,0.0654498470f, 0.97f,0.1308996939f},
    // 3: vowel_pair B (Vowel_Oo)
    {0.96f,0.0078539816f, 0.98f,0.0314159261f, 0.985f,0.0445058960f, 0.992f,0.1308996939f, 0.99f,0.2879793267f, 0.985f,0.3926990818f},
    // 4: bell_pair B (Metallic_Cluster)
    {0.997f,0.5235987756f, 0.996f,0.6283185307f, 0.995f,0.7068583471f, 0.993f,0.9424777961f, 0.991f,1.0995574288f, 0.989f,1.2566370614f},
    // 5: low_pair B (Formant_Pad)
    {0.97f,0.0261799388f, 0.985f,0.0654498470f, 0.99f,0.1570796327f, 0.992f,0.2356194490f, 0.99f,0.3665191429f, 0.988f,0.4712388980f}
};
static const int MORPH_PAIRS[3][2] = {{0,3},{1,4},{2,5}};

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

    void updateCoefficientsBlock(int numSamples);
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

    // RMS auto-makeup state
    float preRMSsq = 1e-6f, postRMSsq = 1e-6f;
    juce::LinearSmoothedValue<float> makeupSm;
};
