#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "MorphicRhythmMatrixProcessor.h"

class MorphicRhythmMatrixEditor : public juce::AudioProcessorEditor,
                                  private juce::Timer
{
public:
    explicit MorphicRhythmMatrixEditor (MorphicRhythmMatrixProcessor&);
    ~MorphicRhythmMatrixEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override { repaint(); }

    MorphicRhythmMatrixProcessor& processor;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MorphicRhythmMatrixEditor)
};


