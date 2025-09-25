#include <array>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <complex>
#include <cstdio>

#include "zplane_engine/DspBridge.h"

namespace
{
    pe::Biquad5 makeTestSection(float radius, float theta)
    {
        pe::Biquad5 s{};
        radius = std::clamp(radius, 0.1f, 0.9999f);
        s.a1 = -2.0f * radius * std::cos(theta);
        s.a2 = radius * radius;
        s.b0 = 1.0f;
        s.b1 = 0.0f;
        s.b2 = 0.0f;
        return s;
    }

    float magnitudeAt(const pe::Biquad5& section, float theta)
    {
        const std::complex<float> e1 = std::polar(1.0f, -theta);
        const std::complex<float> e2 = std::polar(1.0f, -2.0f * theta);
        const std::complex<float> num = section.b0 + section.b1 * e1 + section.b2 * e2;
        const std::complex<float> den = 1.0f + section.a1 * e1 + section.a2 * e2;
        return std::abs(num / den);
    }
}

int main()
{
    std::array<pe::Biquad5, pe::kMaxSections> base{};
    const float baseRadius = 0.7f;
    const float theta = 0.6f;
    base[0] = makeTestSection(baseRadius, theta);

    const float resonances[] { 0.2f, 0.5f, 0.8f };
    float lastMag = 0.0f;
    for (size_t i = 0; i < std::size(resonances); ++i)
    {
        auto sections = base;
        pe::DspBridge::applyResonanceToSections(sections.data(), 1, resonances[i]);

        const float mag = magnitudeAt(sections[0], theta);
        const float scalar = pe::DspBridge::estimatePassivityScalar(sections.data(), 1, 48000.0);

        std::printf("res=%.2f -> mag=%.4f, scalar=%.4f\n", resonances[i], mag, scalar);

        if (i > 0)
        {
            if (!(mag >= lastMag - 0.01f))
                return 1;
        }
        if (!(scalar <= 1.0001f && scalar > 0.0f))
            return 1;

        lastMag = mag;
    }

    return 0;
}
