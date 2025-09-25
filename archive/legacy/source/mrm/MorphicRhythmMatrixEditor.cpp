#include "MorphicRhythmMatrixEditor.h"

MorphicRhythmMatrixEditor::MorphicRhythmMatrixEditor (MorphicRhythmMatrixProcessor& p)
: juce::AudioProcessorEditor (&p), processor (p)
{
    setOpaque(true);
    setWantsKeyboardFocus(true);
    setSize(520, 300);
    startTimerHz(30);
}

MorphicRhythmMatrixEditor::~MorphicRhythmMatrixEditor()
{
    stopTimer();
}

void MorphicRhythmMatrixEditor::paint (juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
    g.setColour(juce::Colours::limegreen);
    g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 14.0f, 0));

    auto* morphParam = processor.getParameters().getRawParameterValue("morph");
    const float morph = morphParam ? morphParam->load() : 0.0f;

    auto area = getLocalBounds();
    const int barWidth = area.getWidth() - 20;
    const int filled = (int) juce::jlimit(0, barWidth, (int) std::round(morph * (float) barWidth));

    juce::String bar;
    bar.preallocateBytes((size_t) barWidth + 2);
    bar << '[';
    for (int i = 0; i < barWidth; ++i)
        bar << (i < filled ? '#' : '.');
    bar << ']';

    juce::String text;
    text << "MORPHIC RHYTHM MATRIX\n> ready\n\n";
    text << bar << "\n";
    text << "morph b1: " << juce::String(morph, 2) << "\n";
    text << "\n> [M] cycle morph modes (stub)";

    g.drawMultiLineText(text, 10, 24, area.getWidth() - 20);
}

void MorphicRhythmMatrixEditor::resized()
{
}


