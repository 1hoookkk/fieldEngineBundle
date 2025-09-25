#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
// Foleys GUI Magic disabled - using minimal stub
// #include <foleys_gui_magic/foleys_gui_magic.h>
#include "../MorphFilter.h"
#include "../EMUFilter.h"
#include "../AtomicOscillator.h"
#include "../../ui/UiStateProvider.h"

class FoleysFieldEngineProcessor : public juce::AudioProcessor,
                                  public UiStateProvider
{
public:
    FoleysFieldEngineProcessor();
    ~FoleysFieldEngineProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "FieldEngineFX (Foleys)"; }
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

    // APVTS access for Foleys GUI Magic
    juce::AudioProcessorValueTreeState& getAPVTS() { return parameters; }
    foleys::MagicProcessorState& getMagicState() { return magicState; }

    // UiStateProvider implementation
    double getSampleRate() const override { return currentSampleRate.load(); }
    float getMasterMorphAlpha() const override { return masterAlpha.load(); }
    bool isBypassed() const override { return bypass.load(); }
    bool isSidechainActive() const override { return sidechain.load(); }
    juce::String getTitle() const override { return "FIELD ENGINE FX"; }

    int getNumBands() const override { return kNumBands; }
    juce::String getBandName(int b) const override { return bandNames[b]; }
    float getBandEnergy(int b) const override { return bandEnergy[b].load(); }
    float getBandMorphAlpha(int b) const override { return bandAlpha[b].load(); }
    float getBandGainDb(int b) const override { return bandGainDb[b].load(); }
    bool isBandMuted(int b) const override { return bandMuted[b].load(); }
    juce::String getBandMorphPath(int b) const override { return bandPath[b]; }

private:
    foleys::MagicProcessorState magicState { *this };
    juce::AudioProcessorValueTreeState parameters;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // DSP components
    MorphFilter morphFilter;
    std::array<AuthenticEMUZPlane, 2> channelFilters{};
    AtomicOscillator lfo;

    // Smoothed parameters
    juce::LinearSmoothedValue<float> morphSmoother;
    juce::LinearSmoothedValue<float> cutoffSmoother;
    juce::LinearSmoothedValue<float> resonanceSmoother;
    juce::LinearSmoothedValue<float> driveSmoother;
    juce::LinearSmoothedValue<float> outputSmoother;

    // UI-visible state (atomic for lock-free cross-thread reads)
    std::atomic<double> currentSampleRate { 48000.0 };
    std::atomic<float> masterAlpha { 0.0f };
    std::atomic<bool> bypass { false };
    std::atomic<bool> sidechain { false };

    static constexpr int kNumBands = 8;
    juce::StringArray bandNames { "SUB","LOW","LOWMID","MID","UPMID","HI","AIR","SPARK" };
    std::array<std::atomic<float>, kNumBands> bandEnergy {};
    std::array<std::atomic<float>, kNumBands> bandAlpha {};
    std::array<std::atomic<float>, kNumBands> bandGainDb {};
    std::array<std::atomic<bool>, kNumBands> bandMuted {};
    std::array<juce::String, kNumBands> bandPath { "LP→BP","BP→HP","HP→LP","LP→NT","NT→BP","BP→PH","PH→HP","LP→COMB" };

    // Custom visualizers for Foleys
    void setupMagicState();
    void updateAnalysisData();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FoleysFieldEngineProcessor)
};