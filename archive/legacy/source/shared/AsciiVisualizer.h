#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_opengl/juce_opengl.h>
#include <vector>
#include <string>
#include <array>

class AsciiVisualizer : public juce::Component, public juce::Timer
{
public:
    AsciiVisualizer();
    ~AsciiVisualizer() override;
    
    enum class Mode
    {
        WIREFRAME,
        WATERFALL,
        PLASMA,
        NUM_MODES
    };
    
    void paint (juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;
    
    void cycleMode();
    
    // Data setters
    void updateFilterResponse(const std::array<float, 32>& response);
    void updateMorphPosition(float morph);
    void updateLFOValue(float lfo);
    void updateEnvelope(float envelope);
    
private:
    void draw3DWireframe(juce::Graphics& g);
    void drawFrequencyWaterfall(juce::Graphics& g);
    void drawDOSPlasma(juce::Graphics& g);
    
    char densityToChar(float magnitude);
    
    Mode currentMode = Mode::WIREFRAME;
    
    // Character gradient for density mapping
    const std::vector<char> gradient = {' ', '.', ':', '-', '=', '+', '*', '#', '%', '@'};

    // Visualization data
    float morphPosition = 0.5f;
    float lfoValue = 0.0f;
    float envelopeValue = 0.0f;
    std::array<float, 32> filterResponse{};
    
    // State for animations
    float rotationAngle = 0.0f;
    std::vector<std::string> waterfallHistory;
    
    // Performance
    juce::Image cache;
    std::vector<std::string> charBuffer;
};
