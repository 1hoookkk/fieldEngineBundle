#include <array>
#include <cassert>
#include <cstdio>

#include "zplane_models/Zmf1Loader.h"
#include "zplane_engine/DspBridge.h"

int main()
{
    std::array<pe::Biquad5, pe::kMaxSections> sections {};
    sections[0] = { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f };

    float unityScalar = pe::DspBridge::estimatePassivityScalar(sections.data(), 1, 48000.0);
    std::printf("unity scalar = %.6f\n", unityScalar);
    if (!(unityScalar <= 1.000001f && unityScalar >= 0.95f))
        return 1;

    sections[0] = { 2.0f, 0.0f, 0.0f, -1.8f, 0.81f };
    float hotScalar = pe::DspBridge::estimatePassivityScalar(sections.data(), 1, 48000.0);
    std::printf("hot scalar = %.6f\n", hotScalar);
    const float floor = 0.35f;
    if (!(hotScalar < unityScalar && hotScalar >= floor))
        return 1;

    return 0;
}
