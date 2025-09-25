#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../shared/EMUFilter.h"
#include "../shared/AtomicOscillator.h"
#include "ADSR.h"

class FieldEngineSynthProcessor : public juce::AudioProcessor
{
public:
    FieldEngineSynthProcessor();
    ~FieldEngineSynthProcessor() override;

    // AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "FieldEngineSynth"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() { return parameters; }

private:
    juce::AudioProcessorValueTreeState parameters;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    static constexpr int MAX_VOICES = 8;
    std::array<AtomicOscillator, MAX_VOICES> voices{};
    std::array<ADSR, MAX_VOICES> envelopes{};
    std::array<bool, MAX_VOICES> voiceActive{};
    std::array<int, MAX_VOICES> voiceNote{};
    std::array<double, MAX_VOICES> voiceStartTime{};
    int nextVoice = 0;

    std::array<AuthenticEMUZPlane, 2> channelFilters{};
    
    juce::LinearSmoothedValue<float> cutoffSmoother;
    juce::LinearSmoothedValue<float> resonanceSmoother;

    float envelopeFollower = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FieldEngineSynthProcessor)
};
