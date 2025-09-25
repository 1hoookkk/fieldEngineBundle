#pragma once
#include <cmath>

struct BiquadSection
{
    float b0=1, b1=0, b2=0, a1=0, a2=0, z1=0, z2=0;
    inline void  reset() noexcept { z1 = z2 = 0; }
    inline float tick (float x) noexcept
    {
        const float y = b0*x + z1;
        z1 = b1*x - a1*y + z2;
        z2 = b2*x - a2*y;

        // Denormal protection
        constexpr float denormEps = 1.0e-20f;
        if (std::abs(z1) < denormEps) z1 = 0.0f;
        if (std::abs(z2) < denormEps) z2 = 0.0f;

        return y;
    }
};

struct BiquadCascade6
{
    BiquadSection s[6];
    void  reset() noexcept { for (auto& q : s) q.reset(); }
    float processSample (float x) noexcept
    {
        for (int i=0; i<6; ++i) x = s[i].tick (x);
        return x;
    }
    static inline void poleToBandpass (float r, float th, BiquadSection& sec)
    {
        float a1 = -2.0f * r * std::cos (th);
        float a2 =  r * r;

        // Raw EMU character: no auto-makeup gain (preserve authentic thin resonance)
        float b0 = 1.0f;

        // clamps for stability in cascade - research shows these won't engage for EMU range
        a1 = std::fmax (-1.999f, std::fmin (1.999f, a1));
        a2 = std::fmax (-0.999f, std::fmin (0.999f, a2));

        sec.a1 = a1;  sec.a2 = a2;
        sec.b0 = b0;  sec.b1 = 0.0f;  sec.b2 = -b0; // zeros at DC & Nyquist
    }
};
