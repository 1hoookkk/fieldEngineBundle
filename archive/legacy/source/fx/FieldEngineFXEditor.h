#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "FieldEngineFXProcessor.h"
#include "../shared/AsciiVisualizer.h"

class FieldEngineFXEditor : public juce::AudioProcessorEditor,
                           private juce::Timer
{
public:
    FieldEngineFXEditor (FieldEngineFXProcessor&);
    ~FieldEngineFXEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    
private:
    void timerCallback() override;
    bool keyPressed (const juce::KeyPress& key) override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;

    void setNormalizedParam (const juce::String& id, float normalized01);
    
    FieldEngineFXProcessor& processor;
    juce::AudioProcessorValueTreeState& valueTreeState;
    AsciiVisualizer visualizer;
    
    juce::Slider morphSlider, intensitySlider, driveSlider, mixSlider, lfoRateSlider, lfoAmountSlider;
    juce::Label morphLabel, intensityLabel, driveLabel, mixLabel, lfoRateLabel, lfoAmountLabel;
    
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> morphAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> intensityAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> driveAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lfoRateAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lfoAmountAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FieldEngineFXEditor)
};
