#include "fieldEngineSynthEditor.h"

fieldEngineSynthEditor::fieldEngineSynthEditor(fieldEngineSynthProcessor& p)
    : AudioProcessorEditor(&p), processor(p),
      terminalFont("Courier New", 16.0f, juce::Font::plain),
      terminalGreen(0xFF00FF00),
      terminalBackground(0xFF0C0C0C)
{
    // Set editor size
    setSize(800, 400);
    setWantsKeyboardFocus(true);

    // Setup terminal styling
    setupTerminalStyling();

    // Add visualizer
    addAndMakeVisible(visualizer);

    // Start 30 FPS timer for smooth visualization
    startTimerHz(30);
}

fieldEngineSynthEditor::~fieldEngineSynthEditor()
{
    stopTimer();
}

void fieldEngineSynthEditor::setupTerminalStyling()
{
    // Configure terminal font and colors
    setLookAndFeel(nullptr); // Use default JUCE look and feel

    // Set the component to be opaque for better performance
    setOpaque(true);
}

void fieldEngineSynthEditor::paint(juce::Graphics& g)
{
    // Fill background with terminal black
    g.fillAll(terminalBackground);

    // Set terminal font and color
    g.setFont(terminalFont);
    g.setColour(terminalGreen);

    // Draw header
    auto headerArea = getLocalBounds().removeFromTop(headerHeight);
    g.drawText("fieldEngine — anything = music",
               headerArea, juce::Justification::centred);

    // Draw footer with controls
    auto footerArea = getLocalBounds().removeFromBottom(footerHeight);
    g.setFont(juce::Font("Courier New", 12.0f, juce::Font::plain));
    g.setColour(juce::Colour(0xFF00FFFF)); // Cyan for instructions

    juce::String controls = "V: Visual Mode (" +
                           juce::String(visualizer.getCurrentMode() == AsciiVisualizer::WIREFRAME ? "WIREFRAME" :
                                       visualizer.getCurrentMode() == AsciiVisualizer::WATERFALL ? "WATERFALL" : "PLASMA") +
                           ") | M: Morph Reset | 8 VOICES";

    g.drawText(controls, footerArea, juce::Justification::centred);

    // Draw parameter status in top corners
    g.setFont(juce::Font("Courier New", 10.0f, juce::Font::plain));
    g.setColour(juce::Colour(0xFF888888)); // Dim green for status

    // Left side - synthesis parameters
    auto& params = processor.getParameters();
    float morph = params.getRawParameterValue("MORPH")->load();
    float detune = params.getRawParameterValue("DETUNE")->load();
    float cutoff = params.getRawParameterValue("CUTOFF")->load();
    float resonance = params.getRawParameterValue("RESONANCE")->load();

    juce::String leftStatus = "MORPH: " + juce::String(morph, 2) +
                             " | DETUNE: " + juce::String(detune, 1) + "st" +
                             " | CUTOFF: " + juce::String(cutoff, 0) + "Hz";

    g.drawText(leftStatus, 10, 5, 500, 20, juce::Justification::left);

    // Right side - envelope parameters
    float attack = params.getRawParameterValue("ATTACK")->load();
    float decay = params.getRawParameterValue("DECAY")->load();
    float sustain = params.getRawParameterValue("SUSTAIN")->load();
    float release = params.getRawParameterValue("RELEASE")->load();

    juce::String rightStatus = "ADSR: " + juce::String(attack, 2) + "s/" +
                              juce::String(decay, 2) + "s/" +
                              juce::String(sustain, 2) + "/" +
                              juce::String(release, 2) + "s";

    g.drawText(rightStatus, getWidth() - 300, 5, 290, 20, juce::Justification::right);

    // Show current envelope value
    g.setColour(juce::Colour(0xFFFFFF00)); // Yellow for live values
    juce::String liveStatus = "ENV: " + juce::String(processor.getCurrentEnvelopeValue(), 3);
    g.drawText(liveStatus, getWidth() - 100, 20, 90, 15, juce::Justification::right);
}

void fieldEngineSynthEditor::resized()
{
    auto area = getLocalBounds();

    // Reserve space for header and footer
    area.removeFromTop(headerHeight);
    area.removeFromBottom(footerHeight);

    // Add small margin
    area.reduce(margin, margin);

    // Set visualizer bounds
    visualizer.setBounds(area);
}

bool fieldEngineSynthEditor::keyPressed(const juce::KeyPress& key)
{
    if (key.getKeyCode() == 'V' || key.getKeyCode() == 'v')
    {
        visualizer.cycleMode();
        repaint(); // Repaint to update the mode display in footer
        return true;
    }

    if (key.getKeyCode() == 'M' || key.getKeyCode() == 'm')
    {
        // Reset morph to center position
        auto* morphParam = processor.getParameters().getParameter("MORPH");
        if (morphParam != nullptr)
        {
            morphParam->setValue(0.5f);
        }
        return true;
    }

    if (key.getKeyCode() == 'R' || key.getKeyCode() == 'r')
    {
        // Reset all parameters to defaults
        auto& params = processor.getParameters();
        params.getParameter("MORPH")->setValue(0.5f);
        params.getParameter("DETUNE")->setValue(0.0f);
        params.getParameter("CUTOFF")->setValue(1000.0f / 20000.0f); // Normalized
        params.getParameter("RESONANCE")->setValue(0.1f);
        params.getParameter("ATTACK")->setValue(0.01f / 5.0f); // Normalized
        params.getParameter("DECAY")->setValue(0.3f / 5.0f);
        params.getParameter("SUSTAIN")->setValue(0.7f);
        params.getParameter("RELEASE")->setValue(1.0f / 10.0f);
        return true;
    }

    return false;
}

void fieldEngineSynthEditor::timerCallback()
{
    // Update visualizer with current processor state
    visualizer.updateMorphPosition(processor.getCurrentMorphPosition());
    visualizer.updateLFOValue(processor.getCurrentLFOValue());
    visualizer.updateEnvelope(processor.getCurrentEnvelopeValue());
    visualizer.updateFilterResponse(processor.getFilterResponse());

    // Trigger visualizer repaint
    visualizer.repaint();

    // Update parameter display
    repaint();
}