#pragma once
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../../pitchEngine/source/dsp/AuthenticEMUZPlane.h"
#include <array>
#include <cmath>

class SecretSauceEngine
{
public:
    void prepare (float sampleRate, int maxBlockSize);
    void setAmount (float amount01);           // 0..1 "how much character"
    void setSpeed (float speedHz);             // 0.1-8 Hz
    void setDepth (float depth01);             // 0-1 depth
    void processStereo (float* left, float* right, int numFrames);
    void reset();

private:
    struct Pole { float r{}, theta{}; };
    struct Biquad
    {
        float b0{}, b1{}, b2{}, a1{}, a2{};
        float z1{}, z2{};
        inline float process(float x) noexcept
        {
            // Direct Form I
            const float y = b0*x + z1;
            z1 = b1*x - a1*y + z2;
            z2 = b2*x - a2*y;
            return y;
        }
        void clear() noexcept { z1 = z2 = 0.f; }
    };

    float fs = 48000.f;
    float amount = 0.7f; // master macro control
    float drive = 1.0f;
    float satAmt = 0.2f;
    float makeup = 1.0f;
    float morph = 0.5f;
    float intensity = 0.6f;

    // Phaser LFO
    float lfoPhase = 0.0f;
    float lfoSpeed = 0.5f;    // Hz
    float lfoDepth = 0.0f;    // 0-1

    // AUTHENTIC EMU Z-PLANE INTEGRATION
    AuthenticEMUZPlane emuFilter;

    // Legacy biquad implementation (kept for fallback)
    std::array<Pole, 6> shapeA48k{};
    std::array<Pole, 6> shapeB48k{};
    std::array<Biquad, 6> leftChain{};
    std::array<Biquad, 6> rightChain{};

    // DC blocking filters
    Biquad leftDCBlock{};
    Biquad rightDCBlock{};

    // Anti-aliasing filters
    Biquad leftAntiAlias{};
    Biquad rightAntiAlias{};

    void loadEmbeddedShapes();      // builds shapeA48k/shapeB48k
    void updateCoefficients();      // calc chain coeffs from shapes/morph/intensity
    void setupDCBlocking();         // setup DC blocking filters
    void setupAntiAliasing();       // setup anti-aliasing filters
    static Pole interpPole (const Pole& a, const Pole& b, float t, float intensity01);
    static void poleToBiquad (const Pole& p, Biquad& s, float intensity01);

    static inline float fastTanh(float x) noexcept { return std::tanh(x); }
};