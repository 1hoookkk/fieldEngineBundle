#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <vector>
#include <functional>

class AsciiVisualizer : public juce::Component,
                       private juce::Timer
{
public:
    enum VizMode
    {
        WIREFRAME = 0,
        WATERFALL,
        PLASMA,
        NUM_MODES,
        // Backward-compat aliases
        MATRIX_CASCADE = WIREFRAME,
        SPECTRAL_WATERFALL = WATERFALL,
        OSCILLOSCOPE_3D = WIREFRAME,
        FILTER_TOPOLOGY = WIREFRAME,
        FREQUENCY_BARS = WATERFALL
    };

    AsciiVisualizer();
    ~AsciiVisualizer() override = default;

    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;

    // Mode control
    void cycleMode();
    void setMode(VizMode mode);
    VizMode getCurrentMode() const { return currentMode; }

    // Data update interface (called from processor)
    void updateFilterResponse(const std::array<float, 32>& response);
    void updateSpectrum(const float* spectrum, int size);
    void updateEnvelope(float env);
    void updateMorphPosition(float morph);
    void updateLFOValue(float lfo);

    // Interactive features
    void setInteractive(bool interactive) { isInteractive = interactive; }
    std::function<void(float, float)> onParameterChange;

private:
    // Timer for animation updates
    void timerCallback() override;

    // Visualization modes (implemented)
    void draw3DWireframe(juce::Graphics& g, const juce::Rectangle<int>& area);
    void drawFrequencyWaterfall(juce::Graphics& g, const juce::Rectangle<int>& area);
    void drawDOSPlasma(juce::Graphics& g, const juce::Rectangle<int>& area);

    // Visual effects (reserved - not currently implemented)
    void drawGlowEffect(juce::Graphics& g, const juce::Rectangle<int>& bounds, juce::Colour color, float intensity);
    void drawScanlines(juce::Graphics& g, const juce::Rectangle<int>& area);
    void drawBorder(juce::Graphics& g, const juce::Rectangle<int>& area);
    void drawStatusBar(juce::Graphics& g, const juce::Rectangle<int>& area);

    // Utilities
    char densityToChar(float value) const;
    juce::String createLine(int width, float* data) const;
    juce::Colour getColorForValue(float value, float alpha = 1.0f) const;
    juce::Point<float> project3D(float x, float y, float z, const juce::Rectangle<int>& area) const;

    // Current mode and state
    VizMode currentMode = WIREFRAME;
    bool isInteractive = false;
    
    // Animation state
    float animationPhase = 0.0f;
    float animationSpeed = 0.05f;
    int frameCounter = 0;
    float plasmaPhase = 0.0f;
    float plasmaSpeed = 0.07f;

    // Visualization data
    std::array<float, 32> filterResponse;
    std::array<float, 64> spectrumData;
    std::array<std::array<float, 128>, 64> waterfallHistory; // Increased resolution
    int waterfallWritePos = 0;
    float envelopeValue = 0.0f;
    float morphPosition = 0.5f;
    float lfoValue = 0.0f;

    // Matrix cascade effect (reserved)
    struct MatrixColumn {
        std::vector<char> chars;
        std::vector<float> brightness;
        float speed;
        int headPosition;
        int trailLength;
    };
    std::vector<MatrixColumn> matrixColumns;

    // Character sets
    const char* gradient = " ·░▒▓█";
    static constexpr int gradientSize = 6;

    // Color scheme - high contrast cyber aesthetic
    juce::Colour primaryGreen = juce::Colour(0, 255, 65);      // Bright matrix green
    juce::Colour secondaryBlue = juce::Colour(0, 150, 255);    // Electric blue
    juce::Colour accentRed = juce::Colour(255, 50, 50);        // Warning red
    juce::Colour backgroundColor = juce::Colour(5, 5, 5);      // Near black
    juce::Colour borderColor = juce::Colour(0, 200, 50);       // Border green

    // Typography
    juce::Font terminalFont;
    juce::Font headerFont;
    float charWidth = 8.0f;
    float lineHeight = 14.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AsciiVisualizer)
};