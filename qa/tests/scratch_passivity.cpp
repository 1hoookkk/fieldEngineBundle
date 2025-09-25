#include <cstdio>
#include "zplane_engine/DspBridge.h"

int main()
{
    pe::DspBridge bridge;
    bridge.setSampleRate(48000.0f);
    if (!bridge.loadModelByBinarySymbol(0)) {
        std::printf("load fail\n");
        return 1;
    }

    pe::ZPlaneParams p{0, 0.5f, 0.8f, 0.5f, 1.0f};
    // We'll call process on a small buffer and dump passivity.
    const int numSamples = 128;
    float left[numSamples]{};
    float right[numSamples]{};
    // impulse
    left[0] = 1.0f;
    float* chans[2] = { left, right };
    bridge.process(chans, 1, numSamples, p);
    for (int i = 0; i < 16; ++i)
        std::printf("%d: %.6f\n", i, left[i]);
    return 0;
}
