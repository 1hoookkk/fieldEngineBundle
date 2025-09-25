#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "MorphFilter.h"
#include "EMUFilter.h"
#include "OptimizedOscillatorPool.h"
#include <memory>
#include <vector>

class fieldEngineSynthProcessor : public juce::AudioProcessor
{
public:
    fieldEngineSynthProcessor();
    ~fieldEngineSynthProcessor() override;

    // AudioProcessor interface
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "fieldEngineSynth"; }

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 1.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int index) override {}
    const juce::String getProgramName(int index) override { return {}; }
    void changeProgramName(int index, const juce::String& newName) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Parameter access
    juce::AudioProcessorValueTreeState& getParameters() { return parameters; }

    // Data access for visualizer
    float getCurrentMorphPosition() const { return currentMorphPosition; }
    float getCurrentLFOValue() const { return currentLFOValue; }
    float getCurrentEnvelopeValue() const { return currentEnvelopeValue; }
    const std::array<float, 32>& getFilterResponse() const { return filterResponse; }

private:
    // Parameter creation
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Parameters
    juce::AudioProcessorValueTreeState parameters;

    // DSP Components (using reference_code)
    std::vector<std::unique_ptr<MorphFilter>> morphFilters;
    std::vector<std::unique_ptr<EMUFilterCore>> emuFilters;  // Use EMUFilterCore instead
    
    // Simple voice management with AtomicOscillator
    static constexpr int MAX_VOICES = 8;
    std::array<AtomicOscillator, MAX_VOICES> voices;
    std::array<bool, MAX_VOICES> voiceActive;
    std::array<int, MAX_VOICES> voiceNote;

    // State tracking
    float currentMorphPosition = 0.5f;
    float currentLFOValue = 0.0f;
    float currentEnvelopeValue = 0.0f;
    std::array<float, 32> filterResponse;

    // Audio processing
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(fieldEngineSynthProcessor)
};