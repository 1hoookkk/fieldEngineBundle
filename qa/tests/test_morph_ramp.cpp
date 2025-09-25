#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <cstdio>
#include <vector>

#include "plugins/morphengine/src/MorphEngineAudioProcessor.h"

static double energyOf(const juce::AudioBuffer<float>& buffer)
{
    double e = 0.0;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            e += (double) buffer.getSample(ch, i) * (double) buffer.getSample(ch, i);
    return e;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    MorphEngineAudioProcessor proc;
    proc.prepareToPlay(48000.0, 128);

    juce::MidiBuffer midi;

    juce::AudioBuffer<float> buffer(2, 128);
    buffer.clear();
    buffer.setSample(0, 0, 1.0f);
    buffer.setSample(1, 0, 1.0f);

    proc.processBlock(buffer, midi);
    double energyBaseline = energyOf(buffer);

    if (auto* morph = proc.apvts.getParameter("zplane.morph"))
    {
        morph->beginChangeGesture();
        morph->setValueNotifyingHost(1.0f);
        morph->endChangeGesture();
    }

    buffer.clear();
    buffer.setSample(0, 0, 1.0f);
    buffer.setSample(1, 0, 1.0f);
    proc.processBlock(buffer, midi);
    double energyAfterChange = energyOf(buffer);

    for (int i = 0; i < 4; ++i)
    {
        buffer.clear();
        buffer.setSample(0, 0, 1.0f);
        buffer.setSample(1, 0, 1.0f);
        proc.processBlock(buffer, midi);
    }
    double energySettled = energyOf(buffer);

    std::printf("baseline=%.6f afterChange=%.6f settled=%.6f\n", energyBaseline, energyAfterChange, energySettled);
", energyBaseline, energyAfterChange, energySettled);

    return (energyAfterChange <= energySettled && energySettled > energyBaseline) ? 0 : 1;
}
