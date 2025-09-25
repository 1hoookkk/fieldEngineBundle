#include "SpectralCanvasProEditor.h"

SpectralCanvasProEditor::SpectralCanvasProEditor(SpectralCanvasProProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(400, 300);
}

SpectralCanvasProEditor::~SpectralCanvasProEditor()
{
}

void SpectralCanvasProEditor::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
    g.setColour(juce::Colours::white);
    g.setFont(15.0f);
    g.drawFittedText("Hello from SpectralCanvasPro!", getLocalBounds(), juce::Justification::centred, 1);
}

void SpectralCanvasProEditor::resized()
{
}
