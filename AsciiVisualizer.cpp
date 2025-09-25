#include "AsciiVisualizer.h"
#include <cmath>

namespace
{
    // TempleOS-inspired color palette
    const juce::Colour bgColor = juce::Colour(0xFF0C0C0C);
    const juce::Colour primaryColor = juce::Colour(0xFF00FF00);
    const juce::Colour highlightColor = juce::Colour(0xFF00FFFF);
}

AsciiVisualizer::AsciiVisualizer()
{
    setOpaque(true);
}

AsciiVisualizer::~AsciiVisualizer()
{
    stopTimer();
}

void AsciiVisualizer::paint(juce::Graphics& g)
{
    g.fillAll(bgColor);
    
    switch (currentMode)
    {
        case Mode::WIREFRAME:
            draw3DWireframe(g);
            break;
        case Mode::WATERFALL:
            drawFrequencyWaterfall(g);
            break;
        case Mode::PLASMA:
            drawDOSPlasma(g);
            break;
    }
    
    // Subtle scanlines
    auto b = getLocalBounds();
    g.setColour(juce::Colour(0x10FFFFFF));
    for (int y = b.getY(); y < b.getBottom(); y += 2)
        g.fillRect(b.getX(), y, b.getWidth(), 1);
    
    // Vignette
    juce::ColourGradient vignette(juce::Colours::transparentBlack,
                                   b.getCentre().toFloat(),
                                   juce::Colour(0x66000000),
                                   b.getTopLeft().toFloat(), true);
    g.setGradientFill(vignette);
    g.fillRect(b);
}

void AsciiVisualizer::resized()
{
    waterfallHistory.assign(getHeight() / 16, std::string(getWidth() / 9, ' '));
    charBuffer.assign(getHeight() / 16, std::string(getWidth() / 9, ' '));
    std::fill(charBuffer.begin(), charBuffer.end(), std::string(getWidth() / 9, '\0'));
}

void AsciiVisualizer::timerCallback()
{
    rotationAngle += lfoValue * 0.1f;
    if (rotationAngle > juce::MathConstants<float>::twoPi)
        rotationAngle -= juce::MathConstants<float>::twoPi;
        
    // Update waterfall
    std::string newLine;
    for(float val : filterResponse)
    {
        newLine += densityToChar(val);
        newLine += densityToChar(val);
    }
    
    if (waterfallHistory.empty()) return;
    
    std::rotate(waterfallHistory.begin(), waterfallHistory.begin() + 1, waterfallHistory.end());
    waterfallHistory.back() = newLine.substr(0, getWidth() / 9);

    repaint();
}

void AsciiVisualizer::cycleMode()
{
    currentMode = static_cast<Mode>(((int)currentMode + 1) % static_cast<int>(Mode::NUM_MODES));
    repaint();
}

void AsciiVisualizer::updateFilterResponse(const std::array<float, 32>& response)
{
    filterResponse = response;
}

void AsciiVisualizer::updateMorphPosition(float morph)
{
    morphPosition = morph;
}

void AsciiVisualizer::updateLFOValue(float lfo)
{
    lfoValue = lfo;
}

void AsciiVisualizer::updateEnvelope(float envelope)
{
    envelopeValue = envelope;
}

char AsciiVisualizer::densityToChar(float magnitude)
{
    int index = static_cast<int>(juce::jlimit(0.0f, 1.0f, magnitude) * (gradient.size() - 1));
    return gradient[static_cast<size_t>(index)];
}

void AsciiVisualizer::draw3DWireframe(juce::Graphics& g)
{
    g.setColour(primaryColor.withAlpha(0.8f + 0.2f * envelopeValue));
    
    auto bounds = getLocalBounds().toFloat();
    auto center = bounds.getCentre();
    
    std::vector<juce::Vector3D<float>> vertices;
    for (int i = 0; i < 8; ++i)
        vertices.push_back({(i & 1) ? -1.0f : 1.0f, (i & 2) ? -1.0f : 1.0f, (i & 4) ? -1.0f : 1.0f});

    float speed = 0.5f + 0.5f * std::abs(lfoValue);
    float ang = rotationAngle * (0.6f + speed * 0.8f);
    auto rotationMatrix = juce::Matrix3D<float>::rotation({0.0f, ang, 0.0f});

    std::vector<juce::Point<float>> projectedPoints;
    for (const auto& vertex : vertices)
    {
        // Manual matrix-vector multiplication for 3D transformation
        auto rotated = juce::Vector3D<float>(
            rotationMatrix.mat[0] * vertex.x + rotationMatrix.mat[4] * vertex.y + rotationMatrix.mat[8]  * vertex.z + rotationMatrix.mat[12],
            rotationMatrix.mat[1] * vertex.x + rotationMatrix.mat[5] * vertex.y + rotationMatrix.mat[9]  * vertex.z + rotationMatrix.mat[13],
            rotationMatrix.mat[2] * vertex.x + rotationMatrix.mat[6] * vertex.y + rotationMatrix.mat[10] * vertex.z + rotationMatrix.mat[14]);
        float scale = (80.0f + 20.0f * envelopeValue) / (2.5f + rotated.z);
        projectedPoints.push_back({center.x + rotated.x * scale, center.y + rotated.y * scale});
    }

    auto drawLine = [&](int a, int b) { g.drawLine(projectedPoints[a].x, projectedPoints[a].y, projectedPoints[b].x, projectedPoints[b].y, 1.5f); };
    drawLine(0, 1); drawLine(1, 3); drawLine(3, 2); drawLine(2, 0);
    drawLine(4, 5); drawLine(5, 7); drawLine(7, 6); drawLine(6, 4);
    drawLine(0, 4); drawLine(1, 5); drawLine(2, 6); drawLine(3, 7);

    // Morph position indicator
    g.setColour(highlightColor.withAlpha(0.8f));
    float indicatorX = center.x + std::cos(morphPosition * juce::MathConstants<float>::twoPi * 2.0f) * 50.0f;
    float indicatorY = center.y + std::sin(morphPosition * juce::MathConstants<float>::twoPi * 2.0f) * 20.0f;
    g.drawText("◆", juce::Rectangle<float>(indicatorX - 8, indicatorY - 8, 16, 16), juce::Justification::centred);
}

void AsciiVisualizer::drawFrequencyWaterfall(juce::Graphics& g)
{
    g.setColour(primaryColor.withAlpha(0.9f));
    g.setFont(juce::FontOptions("Courier New", 16.0f, juce::Font::plain));

    int charHeight = getHeight() / 16;
    int charWidth = getWidth() / 9;

    for (int y = 0; y < charHeight; ++y)
    {
        const std::string& line = waterfallHistory[y];
        if (line != charBuffer[y])
        {
            g.fillRect(0, y * 16, getWidth(), 16);
            g.drawText(line, 0, y * 16, getWidth(), 16, juce::Justification::left, false);
        }
    }

    for(int y = 0; y < charHeight; ++y)
        charBuffer[y] = waterfallHistory[y];
}

void AsciiVisualizer::drawDOSPlasma(juce::Graphics& g)
{
    g.setColour(primaryColor.withAlpha(0.9f));
    g.setFont(juce::FontOptions("Courier New", 16.0f, juce::Font::plain));
    
    float time = (float)juce::Time::getMillisecondCounterHiRes() * 0.001f;
    
    int charWidth = getWidth() / 9;
    int charHeight = getHeight() / 16;

    std::vector<std::string> newBuffer(charHeight, std::string(charWidth, ' '));

    for (int y = 0; y < charHeight; ++y)
    {
        std::string& line = newBuffer[y];
        for (int x = 0; x < charWidth; ++x)
        {
            float val = 0.5f + 0.5f * std::sin(
                (float)x * 0.2f + time + envelopeValue + 
                std::sin((float)y * 0.3f + time) +
                std::sin(std::hypot((float)x/2 - charWidth/2.0f, (float)y - charHeight/2.0f) * 0.2f + time)
            );
            line[x] = densityToChar(val);
        }
    }
    
    g.setColour(primaryColor.withAlpha(0.9f));
    g.setFont(juce::FontOptions("Courier New", 16.0f, juce::Font::plain));

    for (int y = 0; y < charHeight; ++y)
    {
        for (int x = 0; x < charWidth; ++x)
        {
            if (newBuffer[y][x] != charBuffer[y][x])
            {
                g.fillRect(x * 9, y * 16, 9, 16);
                g.drawText(juce::String::charToString(newBuffer[y][x]), x * 9, y * 16, 9, 16, juce::Justification::centred);
            }
        }
    }
    charBuffer = newBuffer;
}
