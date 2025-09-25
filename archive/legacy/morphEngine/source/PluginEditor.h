#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "ui/XtremeLeadLookAndFeel.h"

class MorphEngineEditor : public juce::AudioProcessorEditor,
                          private juce::Timer
{
public:
    explicit MorphEngineEditor(MorphEngineProcessor&);
    ~MorphEngineEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    MorphEngineProcessor& processor;
    MorphEngineUI::XtremeLeadLookAndFeel lookAndFeel;

    // Main LCD display
    MorphEngineUI::XtremeLCDDisplay mainDisplay;
    MorphEngineUI::XtremeLCDDisplay paramDisplay;

    // EMU-style encoders for main controls
    MorphEngineUI::XtremeEncoder morphKnob;
    MorphEngineUI::XtremeEncoder resonanceKnob;
    MorphEngineUI::XtremeEncoder brightnessKnob;
    MorphEngineUI::XtremeEncoder driveKnob;
    MorphEngineUI::XtremeEncoder hardnessKnob;
    MorphEngineUI::XtremeEncoder mixKnob;

    // Style and quality selectors (linear sliders EMU-style)
    juce::Slider styleSlider;
    juce::Slider qualitySlider;

    // Labels with EMU silkscreen style
    juce::Label morphLabel;
    juce::Label resonanceLabel;
    juce::Label brightnessLabel;
    juce::Label driveLabel;
    juce::Label hardnessLabel;
    juce::Label mixLabel;
    juce::Label styleLabel;
    juce::Label qualityLabel;

    // Z-plane visualization placeholder
    class ZPlaneDisplay : public juce::Component
    {
    public:
        void paint(juce::Graphics& g) override;
        void setFilterState(float morph, float resonance, float brightness);
    private:
        float currentMorph = 0.5f;
        float currentResonance = 0.5f;
        float currentBrightness = 0.5f;
    };
    ZPlaneDisplay zplaneDisplay;

    // VU meters EMU style
    class VUMeter : public juce::Component
    {
    public:
        void paint(juce::Graphics& g) override;
        void setLevel(float newLevel) { level = newLevel; repaint(); }
    private:
        float level = 0.0f;
    };
    VUMeter leftMeter;
    VUMeter rightMeter;

    // Attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> styleAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> morphAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> resonanceAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> brightnessAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> driveAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> hardnessAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> qualityAttachment;

    // Helper methods
    void updateDisplays();
    juce::String getStyleName(float value);
    juce::String getQualityName(float value);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MorphEngineEditor)
};