#include <cstdio>
#include <juce_dsp/juce_dsp.h>
#include "api/StaticShapeBank.h"
#include "engines/AuthenticEMUEngine.h"

int main()
{
    try {
        StaticShapeBank shapes;
        AuthenticEMUEngine emu(shapes);

        juce::AudioBuffer<float> buf(1, 64);
        buf.clear();

        // Simple test signal
        for (int n = 0; n < buf.getNumSamples(); ++n)
            buf.getWritePointer(0)[n] = 0.1f * std::sin(0.1f * n);

        emu.prepare(48000.0, buf.getNumSamples(), 1);
        ZPlaneParams p{};
        p.morphPair = 0;
        p.intensity = 0.0f;
        emu.setParams(p);

        std::printf("Engine prepared successfully\n");
        std::printf("Is bypassed: %s\n", emu.isEffectivelyBypassed() ? "yes" : "no");
        std::printf("Num shapes: %d\n", shapes.numShapes());
        std::printf("Num pairs: %d\n", shapes.numPairs());

        emu.processLinear(buf);
        std::printf("Linear processing completed\n");

        return 0;
    } catch (const std::exception& e) {
        std::printf("Exception: %s\n", e.what());
        return 1;
    } catch (...) {
        std::printf("Unknown exception\n");
        return 1;
    }
}