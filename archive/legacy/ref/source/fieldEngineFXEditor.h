#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "fieldEngineFXProcessor.h"
#include "AsciiVisualizer.h"

class fieldEngineFXEditor : public juce::AudioProcessorEditor,
                           private juce::Timer
{
public:
    fieldEngineFXEditor(fieldEngineFXProcessor&);
    ~fieldEngineFXEditor() override;

    // Component interface
    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

private:
    // Timer callback for 30 FPS updates
    void timerCallback() override;

    // UI setup
    void setupTerminalStyling();

    // Reference to processor
    fieldEngineFXProcessor& processor;

    // UI Components
    AsciiVisualizer visualizer;

    // Terminal styling
    juce::Font terminalFont;
    juce::Colour terminalGreen;
    juce::Colour terminalBackground;

    // Layout constants
    static constexpr int headerHeight = 30;
    static constexpr int footerHeight = 25;
    static constexpr int margin = 10;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(fieldEngineFXEditor)
};