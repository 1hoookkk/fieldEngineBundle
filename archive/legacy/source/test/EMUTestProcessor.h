#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "engines/AuthenticEMUEngine.h"
#include "api/StaticShapeBank.h"
#include "wrappers/OversampledEngine.h"

class EMUTestProcessor : public juce::AudioProcessor
{
public:
    EMUTestProcessor();
    ~EMUTestProcessor() override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

private:
    juce::AudioProcessorValueTreeState parameters;
    
    // EMU Engine components
    std::unique_ptr<StaticShapeBank> shapeBank;
    std::unique_ptr<AuthenticEMUEngine> emuEngine;
    std::unique_ptr<OversampledEngine> oversampledEngine;
    
    // Parameters
    juce::AudioParameterFloat* morphParam;
    juce::AudioParameterFloat* intensityParam;
    juce::AudioParameterFloat* driveParam;
    juce::AudioParameterFloat* saturationParam;
    juce::AudioParameterFloat* lfoRateParam;
    juce::AudioParameterFloat* lfoDepthParam;
    juce::AudioParameterChoice* morphPairParam;
    juce::AudioParameterBool* autoMakeupParam;
    juce::AudioParameterBool* bypassParam;
    // Built-in audition tone
    juce::AudioParameterBool* toneOnParam;
    juce::AudioParameterFloat* toneFreqParam;
    juce::AudioParameterFloat* toneLevelDbParam;
    float tonePhase = 0.0f;
    
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
};
