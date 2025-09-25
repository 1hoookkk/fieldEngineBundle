#pragma once
#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "dsp/ZPlaneStyle.h"

class PitchEngineAudioProcessor : public juce::AudioProcessor
{
public:
    PitchEngineAudioProcessor();
    ~PitchEngineAudioProcessor() override = default;

    // Core
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    // Layout
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    // Editor
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;

    // Info
    const juce::String getName() const override { return "pitchEngine Pro"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // Programs (single program)
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    // State
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Params
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts { *this, nullptr, "params", createLayout() };

private:
    void cacheParameterPointers();

    struct CachedParameters
    {
        std::atomic<float>* style = nullptr;
        std::atomic<float>* strength = nullptr;
        std::atomic<float>* retune = nullptr;
        std::atomic<float>* bypass = nullptr;
        std::atomic<float>* secret = nullptr;
    } cachedParams;

    // Secret sauce engine
    ZPlaneStyle zplane;

    // Parameter smoothing for premium feel
    juce::SmoothedValue<float> styleSmoothed;
    juce::SmoothedValue<float> strengthSmoothed;
    juce::SmoothedValue<float> retuneSmoothed;

    // Bypass crossfade (equal-power)
    juce::AudioBuffer<float> dryBypassBuffer;
    juce::LinearSmoothedValue<float> bypassAmount;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchEngineAudioProcessor)
};
