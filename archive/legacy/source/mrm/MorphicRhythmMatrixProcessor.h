#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <array>
#include <cstdint>
#include "../ui/UiStateProvider.h"

class MorphicRhythmMatrixProcessor : public juce::AudioProcessor,
                                     public UiStateProvider
{
public:
    MorphicRhythmMatrixProcessor();
    ~MorphicRhythmMatrixProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MorphicRhythmMatrix"; }
    bool acceptsMidi() const override { return false; }
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

    juce::AudioProcessorValueTreeState& getParameters() { return parameters; }

    // UiStateProvider implementation (lock-free reads for UI)
    double getSampleRate() const override { return currentSampleRate.load(); }
    float  getMasterMorphAlpha() const override { return masterAlpha.load(); }
    bool   isBypassed() const override { return bypass.load(); }
    bool   isSidechainActive() const override { return sidechain.load(); }
    juce::String getTitle() const override { return "MORPHIC RHYTHM MATRIX"; }

    int getNumBands() const override { return kNumBands; }
    juce::String getBandName (int b) const override { return bandNames[b]; }
    float getBandEnergy (int b) const override { return bandEnergy[b].load(); }
    float getBandMorphAlpha (int b) const override { return bandAlpha[b].load(); }
    float getBandGainDb (int b) const override { return bandGainDb[b].load(); }
    bool  isBandMuted (int b) const override { return bandMuted[b].load(); }
    juce::String getBandMorphPath (int b) const override { return bandPath[b]; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState parameters { *this, nullptr, juce::Identifier("MRM"), createParameterLayout() };

    // UI-visible state (atomic for lock-free cross-thread reads)
    std::atomic<double> currentSampleRate { 48000.0 };
    std::atomic<float>  masterAlpha { 0.0f };
    std::atomic<bool>   bypass { false };
    std::atomic<bool>   sidechain { false };

    static constexpr int kNumBands = 8;
    juce::StringArray bandNames { "SUB","LOW","LOWMID","MID","UPMID","HI","AIR","SPARK" };
    std::array<std::atomic<float>, kNumBands> bandEnergy {};
    std::array<std::atomic<float>, kNumBands> bandAlpha {};
    std::array<std::atomic<float>, kNumBands> bandGainDb {};
    std::array<std::atomic<bool>,  kNumBands> bandMuted {};
    std::array<juce::String,       kNumBands> bandPath { "LP→BP","BP→HP","HP→LP","LP→NT","NT→BP","BP→PH","PH→HP","LP→COMB" };

    std::uint64_t sampleCounter { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MorphicRhythmMatrixProcessor)
};


