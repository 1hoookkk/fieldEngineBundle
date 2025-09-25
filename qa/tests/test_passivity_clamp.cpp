#include <array>
#include <cmath>
#include <cstdio>

#include "zplane_engine/DspBridge.h"

int main()
{
    constexpr int numSamples = 1024;
    std::array<float, numSamples> left{};
    std::array<float, numSamples> right{};
    left[0] = 1.0f;
    right[0] = 1.0f;
    float* chans[2] = { left.data(), right.data() };

    pe::DspBridge bridge;
    bridge.setSampleRate(48000.0f);
    if (! bridge.loadModelByBinarySymbol(0))
    {
        std::printf("failed to load model\n");
        return 1;
    }

    pe::ZPlaneParams params;
    params.modelIndex = 0;
    params.morph = 1.0f;
    params.resonance = 1.0f;
    params.cutoff = 0.5f;
    params.mix = 1.0f;

    bridge.process(chans, 2, numSamples, params);

    double maxAbs = 0.0;
    for (int i = 0; i < numSamples; ++i)
    {
        maxAbs = std::max(maxAbs, std::abs((double) left[i]));
        maxAbs = std::max(maxAbs, std::abs((double) right[i]));
    }

    std::printf("maxAbs = %.6f\n", maxAbs);
    return (maxAbs <= 1.15) ? 0 : 1;
}
