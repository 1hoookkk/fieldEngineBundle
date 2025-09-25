#include "fieldEngineFXEditor.h"

fieldEngineFXEditor::fieldEngineFXEditor(fieldEngineFXProcessor& p)
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

fieldEngineFXEditor::~fieldEngineFXEditor()
{
    stopTimer();
}

void fieldEngineFXEditor::setupTerminalStyling()
{
    // Configure terminal font and colors
    setLookAndFeel(nullptr); // Use default JUCE look and feel

    // Set the component to be opaque for better performance
    setOpaque(true);
}

void fieldEngineFXEditor::paint(juce::Graphics& g)
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
                           ") | F: Filter Bypass";

    g.drawText(controls, footerArea, juce::Justification::centred);

    // Draw parameter status in top corners
    g.setFont(juce::Font("Courier New", 10.0f, juce::Font::plain));
    g.setColour(juce::Colour(0xFF888888)); // Dim green for status

    // Left side - current parameter values
    auto& params = processor.getParameters();
    float morph = params.getRawParameterValue("MORPH")->load();
    float lfoRate = params.getRawParameterValue("LFO_RATE")->load();
    float lfoAmount = params.getRawParameterValue("LFO_AMOUNT")->load();
    int syncMode = static_cast<int>(params.getRawParameterValue("LFO_SYNC")->load());

    juce::StringArray syncModes = { "FREE", "1/4", "1/8", "1/16", "1/32" };
    juce::String syncText = syncMode < syncModes.size() ? syncModes[syncMode] : "FREE";

    juce::String leftStatus = "MORPH: " + juce::String(morph, 2) + " | LFO: " +
                             (syncMode == 0 ? juce::String(lfoRate, 1) + "Hz" : syncText) +
                             " | AMT: " + juce::String(lfoAmount, 2);

    g.drawText(leftStatus, 10, 5, 400, 20, juce::Justification::left);

    // Right side - audio status
    juce::String rightStatus = "LFO: " + juce::String(processor.getCurrentLFOValue(), 2) +
                              " | ENV: " + juce::String(processor.getCurrentEnvelopeValue(), 2);

    g.drawText(rightStatus, getWidth() - 250, 5, 240, 20, juce::Justification::right);
}

void fieldEngineFXEditor::resized()
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

bool fieldEngineFXEditor::keyPressed(const juce::KeyPress& key)
{
    if (key.getKeyCode() == 'V' || key.getKeyCode() == 'v')
    {
        visualizer.cycleMode();
        repaint(); // Repaint to update the mode display in footer
        return true;
    }

    if (key.getKeyCode() == 'F' || key.getKeyCode() == 'f')
    {
        // Toggle filter bypass (set morph to center)
        auto* morphParam = processor.getParameters().getParameter("MORPH");
        if (morphParam != nullptr)
        {
            float currentMorph = morphParam->getValue();
            morphParam->setValue(currentMorph < 0.1f ? 0.5f : 0.0f);
        }
        return true;
    }

    return false;
}

void fieldEngineFXEditor::timerCallback()
{
    // Update visualizer with current processor state
    visualizer.updateMorphPosition(processor.getCurrentMorphPosition());
    visualizer.updateLFOValue(processor.getCurrentLFOValue());
    visualizer.updateEnvelope(processor.getCurrentEnvelopeValue());
    visualizer.updateFilterResponse(processor.getFilterResponse());

    // Derive a simple spectrum from filter response for the waterfall view
    {
        const auto& resp = processor.getFilterResponse();
        float spectrum[128] = { 0 };
        for (int i = 0; i < 128; ++i)
        {
            float pos = (float) i * (resp.size() - 1) / 127.0f;
            int i0 = (int) pos;
            int i1 = juce::jmin((int) resp.size() - 1, i0 + 1);
            float frac = pos - (float) i0;
            float v = (1.0f - frac) * resp[i0] + frac * resp[i1];
            // Modulate slightly with envelope for motion
            v = juce::jlimit(0.0f, 1.0f, v * (0.7f + 0.3f * processor.getCurrentEnvelopeValue()));
            spectrum[i] = v;
        }
        visualizer.updateSpectrum(spectrum, 128);
    }

    // Trigger visualizer repaint
    visualizer.repaint();

    // Update parameter display
    repaint();
}