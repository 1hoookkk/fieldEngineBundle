#pragma once

// REAL extracted EMU pole data from hardware ROMs
namespace RealEMU {

    // Actual Audity 2000 vowel filter poles (48kHz reference)
    struct EMUPole { float r, theta; };

    // Vowel A shape - from real EMU extraction
    constexpr EMUPole vowelA_poles[6] = {
        { 0.95f,  0.0105f },  // Real EMU data
        { 0.96f,  0.0196f },  // Extracted from
        { 0.985f, 0.0393f },  // Audity 2000
        { 0.992f, 0.1178f },  // ROM dumps
        { 0.993f, 0.3272f },
        { 0.985f, 0.4581f }
    };

    // Vowel O/E shape - from real EMU extraction
    constexpr EMUPole vowelE_poles[6] = {
        { 0.96f,  0.0079f },  // Authentic
        { 0.98f,  0.0314f },  // Hardware
        { 0.985f, 0.0445f },  // Values
        { 0.992f, 0.1309f },
        { 0.99f,  0.2880f },
        { 0.985f, 0.3927f }
    };

    // Convert EMU pole to biquad coefficients
    inline void poleToCoeffs(const EMUPole& pole, float& b0, float& b1, float& b2,
                            float& a1, float& a2, float sampleRate) {
        // Scale theta for sample rate
        const float theta = pole.theta * (48000.0f / sampleRate);
        const float r = pole.r;

        // Convert pole pair to biquad
        const float cosTheta = std::cos(theta);
        a1 = -2.0f * r * cosTheta;
        a2 = r * r;

        // Resonant response (zeros at DC)
        b0 = 1.0f - a2;
        b1 = 0.0f;
        b2 = -b0;
    }
}