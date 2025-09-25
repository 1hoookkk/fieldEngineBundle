
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

// Our DSP
#include "../../libs/pitchengine_dsp/include/PitchTracker.h"
#include "../../libs/pitchengine_dsp/include/Snapper.h"
#include "../../libs/pitchengine_dsp/include/Shifter.h"
#include "../../libs/pitchengine_dsp/include/SibilantGuard.h"
#include "../../libs/pitchengine_dsp/include/AuthenticEMUZPlane.h"

class HardTuneAudioProcessor : public juce::AudioProcessor
{
public:
    HardTuneAudioProcessor();
    ~HardTuneAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "HardTune"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    // DSP
    PitchTracker   tracker;
    Snapper        snapper;
    Shifter        shifter;
    SibilantGuard  sibilant;
    AuthenticEMUZPlane emu; // EMU Z-plane color stage

    // Buffers
    std::vector<float> ratioBuf;
    std::vector<float> tmpL, tmpR;
    juce::AudioBuffer<float> wetBuf; // stereo wet buffer for color processing

    // State
    double sr = 48000.0;
    int maxBlock = 0;

    // Helpers
    void updateSnapper();
    void updateRanges();
    void updateColor();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HardTuneAudioProcessor)
};
