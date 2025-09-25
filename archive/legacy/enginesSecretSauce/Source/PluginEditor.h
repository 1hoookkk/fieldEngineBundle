#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class PluginEditor : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor (SecretSauceAudioProcessor& p) : juce::AudioProcessorEditor(&p) { setSize(480, 180); }
    void paint (juce::Graphics& g) override { g.fillAll(juce::Colours::black); }
    void resized() override {}
};