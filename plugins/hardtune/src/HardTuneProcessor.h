#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include <atomic>
#include <vector>
#include <limits>
#include <utility>

#include "../../../libs/pitchengine_dsp/include/PitchEngine.h"
#include "../../../libs/pitchengine_dsp/include/Shifter.h"
#include "../../../libs/pitchengine_dsp/include/FormantShifter.h"
#include "../../../libs/pitchengine_dsp/include/SibilantGuard.h"
#include "../../../libs/pitchengine_dsp/include/AuthenticEMUZPlane.h"

class HardTuneUI;

class HardTuneProcessor : public juce::AudioProcessor
{
public:
    HardTuneProcessor();
    ~HardTuneProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "HardTune"; }
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

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::String getCurrentPresetName() const;
    int getNumPresets() const;
    juce::String getPresetName(int index) const;
    void loadPreset(int index);

private:
    // DSP components
    PitchEngine pitchEngine;
    Shifter shifter;
    FormantShifter formantShifter;
    SibilantGuard sibilantGuard;
    AuthenticEMUZPlane colorStage;

    // Working buffers
    std::vector<float> monoBuffer;
    std::vector<float> ratioBuffer;
    juce::AudioBuffer<float> wetBuffer;

    // Cached parameter pointers
    std::atomic<float>* modeParam = nullptr;
    std::atomic<float>* retuneParam = nullptr;
    std::atomic<float>* amountParam = nullptr;
    std::atomic<float>* keyParam = nullptr;
    std::atomic<float>* scaleParam = nullptr;
    std::atomic<float>* colorParam = nullptr;
    std::atomic<float>* formantParam = nullptr;
    std::atomic<float>* throatParam = nullptr;
    std::atomic<float>* mixParam = nullptr;
    std::atomic<float>* biasParam = nullptr;
    std::atomic<float>* inputTypeParam = nullptr;

    // State
    double currentSampleRate = 48000.0;
    int preparedBlockSize = 0;

    int lastKey = -1;
    int lastScale = -1;
    int lastBias = std::numeric_limits<int>::min();
    int lastInputType = -1;
    int lastMode = -1;

    float lastRetune = std::numeric_limits<float>::quiet_NaN();
    float lastColor = std::numeric_limits<float>::quiet_NaN();

    struct Preset
    {
        juce::String name;
        int mode = 0;
        float retune = 1.0f;
        float amount = 1.0f;
        float mix = 1.0f;
        float color = 0.15f;
        float formant = 0.0f;
        float throat = 1.0f;
        int key = 0;
        int scale = 0;
        int bias = 0;
        int inputType = 2;
    };

    std::vector<Preset> factoryPresets;
    int currentPresetIndex = -1;

    void ensureCapacity(int numSamples, int numChannels);
    void refreshKeyScale(bool force = false);
    void refreshRetune(bool force = false);
    void refreshInputRange(bool force = false);
    void refreshMode(bool force = false);
    void updateColor(float colourAmount, bool force = false);
    void initializeFactoryPresets();

    static uint16_t maskForScale(int scaleIndex);
    static int biasForChoice(int choice);
    static std::pair<float, float> rangeForInputType(int typeIndex);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HardTuneProcessor)
};
