#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "MorphEngineAudioProcessor.h"
#include "ResonanceLoom.h"

class PremiumMorphUI : public juce::AudioProcessorEditor,
                       private juce::Timer
{
public:
    explicit PremiumMorphUI (MorphEngineAudioProcessor&);
    ~PremiumMorphUI() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setupLookAndFeel();
    void createControls();

    // Clean vector-based drawing methods
    void drawBackground (juce::Graphics& g);
    void drawKnob (juce::Graphics& g, const juce::Rectangle<float>& bounds, float value, const juce::String& label);
    void drawSlider (juce::Graphics& g, const juce::Rectangle<float>& bounds, float value, const juce::String& label);

    MorphEngineAudioProcessor& processor;

    // Main controls
    juce::Slider morphSlider;
    juce::Slider resonanceSlider;
    juce::Slider mixSlider;
    juce::Slider driveSlider;

    // Labels
    juce::Label morphLabel;
    juce::Label resonanceLabel;
    juce::Label mixLabel;
    juce::Label driveLabel;
    juce::Label titleLabel;

    // Preset selection
    juce::ComboBox presetCombo;

    // Parameter attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> morphAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> resonanceAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> driveAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> presetAttachment;

    // Frequency response display
    std::unique_ptr<ResonanceLoom> spectrumDisplay;

    // Layout areas
    juce::Rectangle<int> headerArea;
    juce::Rectangle<int> spectrumArea;
    juce::Rectangle<int> controlsArea;
    juce::Rectangle<int> presetArea;

    // Color palette - UAD inspired
    static const juce::Colour backgroundDark;
    static const juce::Colour panelDark;
    static const juce::Colour accentBlue;
    static const juce::Colour textWhite;
    static const juce::Colour textGrey;
    static const juce::Colour knobRing;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PremiumMorphUI)
};