
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

class HardTuneAudioProcessor;

class HardTuneAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit HardTuneAudioProcessorEditor (HardTuneAudioProcessor&);
    ~HardTuneAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    HardTuneAudioProcessor& processor;

    // Clean controls (MetaTune-style minimalism)
    juce::ComboBox modeBox, keyBox, scaleBox, biasBox, inputTypeBox;
    juce::Slider retune, amount, color, formant, throat, mix;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeA, keyA, scaleA, biasA, inputA;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>    retuneA, amountA, colorA, formantA, throatA, mixA;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HardTuneAudioProcessorEditor)
};
