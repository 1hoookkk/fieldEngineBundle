#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "FieldEngineSynthProcessor.h"
#include "../shared/AsciiVisualizer.h"

class FieldEngineSynthEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit FieldEngineSynthEditor (FieldEngineSynthProcessor&);
    ~FieldEngineSynthEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    FieldEngineSynthProcessor& processor;
    juce::AudioProcessorValueTreeState& valueTreeState;
    AsciiVisualizer visualizer;
    
    juce::Slider detuneSlider, cutoffSlider, resonanceSlider;
    juce::Slider attackSlider, decaySlider, sustainSlider, releaseSlider;
    juce::Label detuneLabel, cutoffLabel, resonanceLabel;
    juce::Label attackLabel, decayLabel, sustainLabel, releaseLabel;
    
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> detuneAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> cutoffAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> resonanceAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> decayAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sustainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> releaseAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FieldEngineSynthEditor)
};
