#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "../libs/emu_engine/include/AuthenticEMUZPlane.h"

class MorphEngineProcessor : public juce::AudioProcessor
{
public:
    MorphEngineProcessor();
    ~MorphEngineProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "morphEngine"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    // Parameters
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts { *this, nullptr, "params", createLayout() };

private:
    // EMU Engine
    AuthenticEMUZPlane emuEngine;

    // Parameter smoothing
    juce::SmoothedValue<float> morphSmoothed;
    juce::SmoothedValue<float> resonanceSmoothed;
    juce::SmoothedValue<float> brightnessSmoothed;
    juce::SmoothedValue<float> driveSmoothed;
    juce::SmoothedValue<float> hardnessSmoothed;
    juce::SmoothedValue<float> mixSmoothed;

    // Processing state
    int currentStyle = 1; // Default to Velvet
    bool isTrackMode = true;

    // Dry buffer for mix
    juce::AudioBuffer<float> dryBuffer;

    // Helper methods
    void updateStylePreset(int styleIndex);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MorphEngineProcessor)
};
