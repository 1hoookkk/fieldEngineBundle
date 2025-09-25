#include "AsciiVisualizer.h"
#include <cmath>

AsciiVisualizer::AsciiVisualizer()
    : terminalFont("Courier New", 14.0f, juce::Font::plain)
{
    // Initialize data arrays
    filterResponse.fill(0.0f);
    for (auto& line : waterfallHistory) {
        line.fill(0.0f);
    }

    // Set up font metrics for ASCII art
    charWidth = terminalFont.getStringWidth("M");
    lineHeight = terminalFont.getHeight();
}

void AsciiVisualizer::paint(juce::Graphics& g)
{
    // Set terminal styling
    g.fillAll(juce::Colour(0xFF0C0C0C)); // Dark terminal background
    g.setFont(terminalFont);
    g.setColour(juce::Colours::lime);

    auto area = getLocalBounds();

    switch (currentMode)
    {
        case WIREFRAME:
            draw3DWireframe(g, area);
            break;
        case WATERFALL:
            drawFrequencyWaterfall(g, area);
            break;
        case PLASMA:
            drawDOSPlasma(g, area);
            break;
        default:
            break;
    }
}

void AsciiVisualizer::resized()
{
    // Update character grid dimensions
    charWidth = terminalFont.getStringWidth("M");
    lineHeight = terminalFont.getHeight();
}

void AsciiVisualizer::cycleMode()
{
    currentMode = static_cast<VizMode>((currentMode + 1) % NUM_MODES);
}

void AsciiVisualizer::setMode(VizMode mode)
{
    currentMode = mode;
    repaint();
}

void AsciiVisualizer::mouseDown(const juce::MouseEvent& event)
{
    if (event.mods.isRightButtonDown())
    {
        cycleMode();
        repaint();
        return;
    }

    if (isInteractive && onParameterChange)
    {
        // Map click position to two generic parameters in [0,1]
        auto bounds = getLocalBounds();
        float xNorm = juce::jlimit(0.0f, 1.0f, (event.position.x - bounds.getX()) / (float) juce::jmax(1, bounds.getWidth()));
        float yNorm = juce::jlimit(0.0f, 1.0f, (event.position.y - bounds.getY()) / (float) juce::jmax(1, bounds.getHeight()));
        onParameterChange(xNorm, yNorm);
    }
}

void AsciiVisualizer::mouseDrag(const juce::MouseEvent& event)
{
    if (isInteractive && onParameterChange)
    {
        auto bounds = getLocalBounds();
        float xNorm = juce::jlimit(0.0f, 1.0f, (event.position.x - bounds.getX()) / (float) juce::jmax(1, bounds.getWidth()));
        float yNorm = juce::jlimit(0.0f, 1.0f, (event.position.y - bounds.getY()) / (float) juce::jmax(1, bounds.getHeight()));
        onParameterChange(xNorm, yNorm);
        repaint();
    }
}

void AsciiVisualizer::timerCallback()
{
    // If started externally, advance simple animation state and repaint
    animationPhase += animationSpeed;
    frameCounter++;
    repaint();
}

void AsciiVisualizer::updateFilterResponse(const std::array<float, 32>& response)
{
    filterResponse = response;
}

void AsciiVisualizer::updateSpectrum(const float* spectrum, int size)
{
    if (spectrum == nullptr || size <= 0) return;

    // Copy spectrum data to current waterfall line
    int copySize = juce::jmin(size, static_cast<int>(waterfallHistory[waterfallWritePos].size()));
    for (int i = 0; i < copySize; ++i) {
        waterfallHistory[waterfallWritePos][i] = spectrum[i];
    }

    // Advance write position
    waterfallWritePos = (waterfallWritePos + 1) % waterfallHistory.size();
}

void AsciiVisualizer::updateEnvelope(float env)
{
    envelopeValue = juce::jlimit(0.0f, 1.0f, env);
}

void AsciiVisualizer::updateMorphPosition(float morph)
{
    morphPosition = juce::jlimit(0.0f, 1.0f, morph);
}

void AsciiVisualizer::updateLFOValue(float lfo)
{
    lfoValue = juce::jlimit(-1.0f, 1.0f, lfo);
}

char AsciiVisualizer::densityToChar(float magnitude) const
{
    magnitude = juce::jlimit(0.0f, 1.0f, magnitude);
    int index = static_cast<int>(magnitude * (gradientSize - 1));
    return gradient[index];
}

juce::String AsciiVisualizer::createLine(int width, float* data) const
{
    juce::String line;
    int numChars = width / static_cast<int>(charWidth);

    for (int i = 0; i < numChars; ++i) {
        float value = (data != nullptr && i < 64) ? data[i] : 0.0f;
        line += densityToChar(value);
    }

    return line;
}

void AsciiVisualizer::draw3DWireframe(juce::Graphics& g, const juce::Rectangle<int>& area)
{
    int centerX = area.getCentreX();
    int centerY = area.getCentreY();

    // Create animated wireframe cube affected by LFO and morph
    float rotation = lfoValue * 0.5f + morphPosition * 0.3f;
    float scale = 1.0f + envelopeValue * 0.3f;

    juce::StringArray cubeLines;
    cubeLines.add("        ╭─────────╮");
    cubeLines.add("       ╱│         │╱");
    cubeLines.add("      ╱ │    ◆    │ ╱");
    cubeLines.add("     ╱  │         │╱");
    cubeLines.add("    ╱   ╰─────────╯");
    cubeLines.add("   ╱   ╱│       │╱");
    cubeLines.add("  ╱   ╱ │   ●   │");
    cubeLines.add(" ╱   ╱  │       │");
    cubeLines.add("╱   ╱   ╰───────╯");
    cubeLines.add("   ╱");

    // Add morph indicator
    cubeLines.add("");
    cubeLines.add("MORPH: " + juce::String(morphPosition, 2));
    cubeLines.add("LFO:   " + juce::String(lfoValue, 2));
    cubeLines.add("ENV:   " + juce::String(envelopeValue, 2));

    // Apply rotation offset
    int offsetX = static_cast<int>(std::cos(rotation) * 20.0f * scale);
    int offsetY = static_cast<int>(std::sin(rotation) * 10.0f * scale);

    for (int i = 0; i < cubeLines.size(); ++i) {
        int x = centerX + offsetX - 60;
        int y = centerY + offsetY - (cubeLines.size() * static_cast<int>(lineHeight)) / 2 + i * static_cast<int>(lineHeight);
        g.drawText(cubeLines[i], x, y, 120, static_cast<int>(lineHeight), juce::Justification::centred);
    }
}

void AsciiVisualizer::drawFrequencyWaterfall(juce::Graphics& g, const juce::Rectangle<int>& area)
{
    int numLines = area.getHeight() / static_cast<int>(lineHeight);
    int numChars = area.getWidth() / static_cast<int>(charWidth);

    // Draw waterfall from top to bottom (newest to oldest)
    for (int line = 0; line < juce::jmin(numLines, static_cast<int>(waterfallHistory.size())); ++line) {
        int historyIndex = (waterfallWritePos - line - 1 + waterfallHistory.size()) % waterfallHistory.size();

        juce::String lineText;
        for (int i = 0; i < juce::jmin(numChars, static_cast<int>(waterfallHistory[historyIndex].size())); ++i) {
            float magnitude = waterfallHistory[historyIndex][i];
            // Apply envelope modulation to brightness
            magnitude *= (0.5f + envelopeValue * 0.5f);
            lineText += densityToChar(magnitude);
        }

        int y = area.getY() + line * static_cast<int>(lineHeight);
        g.drawText(lineText, area.getX(), y, area.getWidth(), static_cast<int>(lineHeight), juce::Justification::left);
    }

    // Draw status line
    juce::String status = "FREQ WATERFALL | MORPH: " + juce::String(morphPosition, 2);
    g.drawText(status, area.getX(), area.getBottom() - static_cast<int>(lineHeight),
               area.getWidth(), static_cast<int>(lineHeight), juce::Justification::left);
}

void AsciiVisualizer::drawDOSPlasma(juce::Graphics& g, const juce::Rectangle<int>& area)
{
    plasmaPhase += plasmaSpeed;

    int numLines = area.getHeight() / static_cast<int>(lineHeight);
    int numChars = area.getWidth() / static_cast<int>(charWidth);

    for (int y = 0; y < numLines; ++y) {
        juce::String line;

        for (int x = 0; x < numChars; ++x) {
            // Calculate plasma effect
            float value1 = std::sin(x * 0.1f + plasmaPhase);
            float value2 = std::cos(y * 0.08f + plasmaPhase * 0.7f);
            float value3 = std::sin(std::sqrt(x*x + y*y) * 0.05f + plasmaPhase * 1.2f);

            // Combine with audio data
            float plasma = (value1 + value2 + value3) / 3.0f;
            plasma += morphPosition * 0.5f; // Morph affects overall brightness
            plasma += lfoValue * 0.3f;      // LFO adds movement
            plasma += envelopeValue * 0.4f; // Envelope adds intensity

            // Normalize to 0-1 range
            plasma = (plasma + 1.0f) * 0.5f;
            plasma = juce::jlimit(0.0f, 1.0f, plasma);

            line += densityToChar(plasma);
        }

        int yPos = area.getY() + y * static_cast<int>(lineHeight);
        g.drawText(line, area.getX(), yPos, area.getWidth(), static_cast<int>(lineHeight), juce::Justification::left);
    }

    // Draw status line
    juce::String status = "DOS PLASMA | LFO: " + juce::String(lfoValue, 2) + " | ENV: " + juce::String(envelopeValue, 2);
    g.drawText(status, area.getX(), area.getBottom() - static_cast<int>(lineHeight),
               area.getWidth(), static_cast<int>(lineHeight), juce::Justification::left);
}