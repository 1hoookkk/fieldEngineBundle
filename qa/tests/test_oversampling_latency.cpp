#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <cstdio>

#include "plugins/morphengine/src/MorphEngineAudioProcessor.h"

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    MorphEngineAudioProcessor proc;
    proc.prepareToPlay (48000.0, 256);

    if (proc.getLatencySamples() != 0)
    {
        std::printf ("expected latency 0 in Track mode, got %d\n", proc.getLatencySamples());
        return 1;
    }

    if (auto* quality = proc.apvts.getParameter ("quality.mode"))
    {
        quality->beginChangeGesture();
        quality->setValueNotifyingHost (1.0f);
        quality->endChangeGesture();
    }

    juce::AudioBuffer<float> buffer (2, 256);
    buffer.clear();
    juce::MidiBuffer midi;

    proc.processBlock (buffer, midi);

    const int latency = proc.getLatencySamples();
    std::printf ("latency print=%d\n", latency);
    if (latency <= 0)
        return 1;

    // Toggle back to Track and ensure latency resets
    if (auto* quality = proc.apvts.getParameter ("quality.mode"))
    {
        quality->beginChangeGesture();
        quality->setValueNotifyingHost (0.0f);
        quality->endChangeGesture();
    }

    proc.processBlock (buffer, midi);
    std::printf ("latency after returning to Track=%d\n", proc.getLatencySamples());
    return proc.getLatencySamples() == 0 ? 0 : 1;
}
