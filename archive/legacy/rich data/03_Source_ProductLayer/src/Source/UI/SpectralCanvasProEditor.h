#pragma once
#include <JuceHeader.h>
#include "SpectralCanvasProProcessor.h"

class SpectralCanvasProEditor : public juce::AudioProcessorEditor
{
public:
    explicit SpectralCanvasProEditor(SpectralCanvasProProcessor&);
    ~SpectralCanvasProEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    SpectralCanvasProProcessor& processor;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectralCanvasProEditor)
};
