#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "MorphFilter.h"
#include "EMUFilter.h"
#include "AtomicOscillator.h"
#include <memory>
#include <vector>

class fieldEngineFXProcessor : public juce::AudioProcessor
{
public:
    enum SyncMode
    {
        FREE = 0,
        QUARTER,    // 1/4 note
        EIGHTH,     // 1/8 note
        SIXTEENTH,  // 1/16 note
        THIRTYSECOND // 1/32 note
    };

    fieldEngineFXProcessor();
    ~fieldEngineFXProcessor() override;

    // AudioProcessor interface
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "fieldEngineFX"; }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

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

    // Tempo sync utilities
    float getSyncMultiplier(SyncMode mode) const;
    void updateLFOFrequency();
    void updateFilterResponse();

    // Parameters
    juce::AudioProcessorValueTreeState parameters;

    // DSP Components (using reference_code)
    std::vector<std::unique_ptr<MorphFilter>> morphFilters;
    std::vector<std::unique_ptr<EMUFilterCore>> emuFilters;  // Use EMUFilterCore instead
    AtomicOscillator lfo;

    // State tracking
    SyncMode currentSyncMode = FREE;
    float currentMorphPosition = 0.5f;
    float currentLFOValue = 0.0f;
    float currentEnvelopeValue = 0.0f;
    std::array<float, 32> filterResponse;

    // Host tempo info
    double currentSampleRate = 44100.0;
    double hostTempo = 120.0;
    bool isPlaying = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(fieldEngineFXProcessor)
};