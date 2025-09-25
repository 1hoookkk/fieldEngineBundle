#include <array>
#include <cmath>
#include <cstdio>

#include "zplane_engine/DspBridge.h"

int main()
{
    pe::DspBridge bridge;
    bridge.setSampleRate(48000.0f);

    if (!bridge.loadModelByBinarySymbol(0))
    {
        std::printf("model load failed\n");
        return 1;
    }

    constexpr int numSamples = 512;
    std::array<float, numSamples> in{};
    std::array<float, numSamples> wet{};
    in[0] = 1.0f;
    wet = in;

    pe::ZPlaneParams params { 0, 0.5f, 0.8f, 0.5f, 1.0f };
    float* chans[1] = { wet.data() };
    bridge.process(chans, 1, numSamples, params);

    double diffEnergy = 0.0;
    for (int i = 0; i < numSamples; ++i)
    {
        double diff = static_cast<double>(wet[i]) - static_cast<double>(in[i]);
        diffEnergy += diff * diff;
    }

    std::printf("diffEnergy = %.9f\n", diffEnergy);
    return diffEnergy > 1e-6 ? 0 : 1;
}
