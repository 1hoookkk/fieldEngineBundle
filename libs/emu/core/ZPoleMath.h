#pragma once
#include <complex>
#include <cmath>
#include <algorithm>

namespace zpm
{
    constexpr float kRefFs = 48000.0f;
    constexpr float kPi    = 3.14159265358979323846f;
    constexpr float kTwoPi = 6.28318530717958647692f;

    inline float wrapAngle (float a)
    {
        while (a >  kPi) a -= kTwoPi;
        while (a < -kPi) a += kTwoPi;
        return a;
    }
    inline float interpAngleShortest (float a, float b, float t)
    {
        return a + t * wrapAngle (b - a);
    }

    // Proper bilinear transform: z@48k -> s -> z@fs
    // This preserves formant frequencies correctly across sample rates
    inline std::complex<float> remap48kToFs (std::complex<float> z48, float fs)
    {
        using C = std::complex<float>;
        const C one { 1.0f, 0.0f };

        // Step 1: z48 -> s domain using inverse bilinear transform
        // s = 2*fs_ref * (z - 1) / (z + 1)
        const auto s = 2.0f * kRefFs * (z48 - one) / (z48 + one);

        // Step 2: s -> z@fs using forward bilinear transform
        // z = (2*fs + s) / (2*fs - s)
        const auto zfs = (2.0f * fs + s) / (2.0f * fs - s);

        return zfs;
    }

    // Helper: convert r,theta pair with proper SR mapping
    inline std::pair<float,float> remapPolar48kToFs(float r48, float theta48, float fs)
    {
        if (fs == kRefFs) return {r48, theta48}; // No remapping needed

        const auto z48 = std::polar(r48, theta48);
        const auto zfs = remap48kToFs(z48, fs);

        const float rfs = std::clamp(std::abs(zfs), 0.10f, 0.9995f);
        const float thetafs = std::arg(zfs);

        return {rfs, wrapAngle(thetafs)};
    }
}
