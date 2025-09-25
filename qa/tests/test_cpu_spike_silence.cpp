#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_core/juce_core.h>
#include <cstdio>

#include "libs/pitchengine_dsp/include/AuthenticEMUZPlane.h"

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    AuthenticEMUZPlane plane;
    plane.prepareToPlay(48000.0);
    plane.setIntensity(1.0f);
    plane.setMorphPosition(0.5f);
    plane.setSectionSaturation(0.05f);

    constexpr int blockSize = 256;
    juce::AudioBuffer<float> buffer(2, blockSize);

    const double ticksPerSecond = juce::Time::getHighResolutionTicksPerSecond();
    double maxBlockMs = 0.0;
    float maxAbs = 0.0f;

    for (int iteration = 0; iteration < 512; ++iteration)
    {
        buffer.clear();
        const auto start = juce::Time::getHighResolutionTicks();
        plane.process(buffer);
        const auto end = juce::Time::getHighResolutionTicks();

        const double elapsedMs = 1000.0 * static_cast<double>(end - start) / ticksPerSecond;
        maxBlockMs = juce::jmax(maxBlockMs, elapsedMs);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const auto* data = buffer.getReadPointer(ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                maxAbs = juce::jmax(maxAbs, std::abs(data[i]));
        }
    }

    const bool timeOk = maxBlockMs <= 1.5;
    const bool noiseOk = maxAbs <= 1.0e-4f;
    std::printf("cpu_silence max_ms=%.3f max_abs=%g -> %s\n", maxBlockMs, maxAbs, (timeOk && noiseOk) ? "ok" : "fail");
    return (timeOk && noiseOk) ? 0 : 2;
}
