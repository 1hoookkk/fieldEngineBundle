#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "MorphEngineAudioProcessor.h"
#include <array>
#include <atomic>

//==============================================================================
/**
 * Modern minimal morphEngine UI
 * Features: Clean preset browser, draggable amount control
 * Size: 500x200px for modern studio integration
 */
class TerminalMorphUI : public juce::AudioProcessorEditor,
                        private juce::Timer,
                        private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit TerminalMorphUI(MorphEngineAudioProcessor&);
    ~TerminalMorphUI() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;

private:
    MorphEngineAudioProcessor& processor;

    // Terminal color scheme
    static const juce::Colour terminalBlue;
    static const juce::Colour terminalBlack;
    static const juce::Colour terminalGreen;
    static const juce::Colour terminalYellow;
    static const juce::Colour terminalRed;
    static const juce::Colour terminalWhite;

    // Layout areas
    juce::Rectangle<int> titleArea, visualizerArea;
    juce::Rectangle<int> presetArea, sliderArea;

    // Real-time filter response visualization
    std::array<float, 128> frequencyResponse{};
    std::atomic<float> currentAmount{0.0f};
    int selectedPreset{0};

    // APVTS attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amountAttachment;

    // UI Components
    juce::Slider amountSlider;
    juce::Label amountLabel;
    std::array<juce::TextButton, 6> presetButtons;

    // Helper methods
    void updateFrequencyResponse();
    void drawTitleBar(juce::Graphics& g, const juce::Rectangle<int>& bounds);
    void drawFrequencyResponse(juce::Graphics& g, const juce::Rectangle<int>& bounds);
    void handlePresetButton(int buttonIndex);

    // Parameter handling
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TerminalMorphUI)
};
