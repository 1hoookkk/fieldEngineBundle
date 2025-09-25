#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>
#include <vector>

class HardTuneProcessor;

class HardTuneUI : public juce::AudioProcessorEditor,
                   private juce::Timer
{
public:
    explicit HardTuneUI(HardTuneProcessor&);
    ~HardTuneUI() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    HardTuneProcessor& processor;

    juce::ComboBox modeBox, keyBox, scaleBox, biasBox, inputTypeBox;
    juce::Slider   retuneSlider, amountSlider, colorSlider, formantSlider, throatSlider, mixSlider;

    juce::ComboBox categoryBox;
    juce::ComboBox presetBox;
    juce::TextButton prevPreset { "<" }, nextPreset { ">" };

    juce::TextButton naturalBtn { "Natural" };
    juce::TextButton tightBtn   { "Tight" };
    juce::TextButton hardBtn    { "Hard" };

    juce::Label retuneLabel, amountLabel, colorLabel, formantLabel, throatLabel, mixLabel;

    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    std::unique_ptr<ComboAttachment> modeAttachment, keyAttachment, scaleAttachment, biasAttachment, inputTypeAttachment;
    std::unique_ptr<SliderAttachment> retuneAttachment, amountAttachment, colorAttachment, formantAttachment, throatAttachment, mixAttachment;

    void configureCombo(juce::ComboBox& box, const juce::String& paramID,
                        std::unique_ptr<ComboAttachment>& attachment);
    void configureSlider(juce::Slider& slider, juce::Label& label,
                         const juce::String& name, const juce::String& paramID,
                         std::unique_ptr<SliderAttachment>& attachment);

    void applyMacroStep(int step, bool withGesture = true);
    void setParamFloat(const juce::String& paramID, float value, bool withGesture = true);
    void updateMacroButtons();
    int  detectMacroStep() const;
    void refreshPresetList();

    juce::String currentCategory { "ALL" };
    juce::String lastPresetName;
    std::vector<int> filteredPresetIndices;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HardTuneUI)
};

