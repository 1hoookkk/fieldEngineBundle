#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>

// Forward declare the Faust-generated class
class mydsp;

class FaustZPlaneProcessor : public juce::AudioProcessor
{
public:
    FaustZPlaneProcessor();
    ~FaustZPlaneProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }

    const juce::String getName() const override { return "FaustZPlane"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Parameter access for integration with existing FieldEngineFX
    void setMorph(float value);
    void setCutoff(float hz);
    void setResonance(float q);
    void setDrive(float db);
    void setMix(float mix);
    void setLFORate(float hz);
    void setLFODepth(float depth);

private:
    std::unique_ptr<mydsp> faustProcessor;
    juce::AudioProcessorValueTreeState parameters;

    // Faust parameter indices (to be filled after code generation)
    int morphIndex = -1;
    int cutoffIndex = -1;
    int resonanceIndex = -1;
    int driveIndex = -1;
    int mixIndex = -1;
    int lfoRateIndex = -1;
    int lfoDepthIndex = -1;

    void updateFaustParameters();
    void initializeParameterIndices();
    
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FaustZPlaneProcessor)
};