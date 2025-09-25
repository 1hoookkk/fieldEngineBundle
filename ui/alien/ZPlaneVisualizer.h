#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_opengl/juce_opengl.h>
#include "EMUColorPalette.h"
#include <array>
#include <complex>

namespace AlienUI
{
    class ZPlaneVisualizer : public juce::Component,
                            private juce::Timer,
                            private juce::OpenGLRenderer
    {
    public:
        ZPlaneVisualizer();
        ~ZPlaneVisualizer() override;
        
        // Component overrides
        void paint(juce::Graphics& g) override;
        void resized() override;
        void mouseMove(const juce::MouseEvent& event) override;
        void mouseDown(const juce::MouseEvent& event) override;
        void mouseDrag(const juce::MouseEvent& event) override;
        void mouseUp(const juce::MouseEvent& event) override;
        
        // OpenGL overrides
        void newOpenGLContextCreated() override;
        void renderOpenGL() override;
        void openGLContextClosing() override;
        
        // Filter coefficient updates
        struct FilterCoefficients
        {
            std::array<std::complex<float>, 5> b{};  // Numerator (zeros)
            std::array<std::complex<float>, 5> a{};  // Denominator (poles)
            int numZeros = 0;
            int numPoles = 0;
        };
        
        void updateCoefficients(const FilterCoefficients& coeffs);
        void setMorphPosition(float position);
        void setResonance(float resonance);
        void setFilterType(int type);
        
        // Visualization options
        void setShowGrid(bool show) { showGrid = show; }
        void setShowFrequencyResponse(bool show) { showFreqResponse = show; }
        void setShowPhaseResponse(bool show) { showPhaseResponse = show; }
        void setInteractive(bool interactive) { isInteractive = interactive; }
        
        // Callbacks
        std::function<void(float freq, float res)> onPoleChanged;
        std::function<void(float freq, float res)> onZeroChanged;
        
    private:
        void timerCallback() override;
        void renderBackground(juce::Graphics& g);
        void renderGrid(juce::Graphics& g);
        void renderUnitCircle(juce::Graphics& g);
        void renderCoefficients(juce::Graphics& g);
        void renderFrequencyResponse(juce::Graphics& g);
        void renderAlienEffects(juce::Graphics& g);
        void renderConstellationConnections(juce::Graphics& g);
        
        // Coordinate conversion
        juce::Point<float> complexToScreen(const std::complex<float>& c) const;
        std::complex<float> screenToComplex(juce::Point<float> p) const;
        float getFrequencyAtPoint(juce::Point<float> p) const;
        
        // Animation helpers
        void animateCoefficients();
        void updateEnergyField();
        
        // OpenGL context
        juce::OpenGLContext openGLContext;
        bool useOpenGL = true;
        
        // Filter state
        FilterCoefficients currentCoeffs;
        FilterCoefficients targetCoeffs;
        float morphPosition = 0.0f;
        float resonanceAmount = 0.5f;
        int filterTypeIndex = 0;
        
        // Visualization state
        bool showGrid = true;
        bool showFreqResponse = true;
        bool showPhaseResponse = false;
        bool isInteractive = true;
        
        // Animation state
        struct AnimatedPoint
        {
            juce::Point<float> current;
            juce::Point<float> target;
            float velocity = 0.0f;
            float energy = 0.0f;
            float pulsePhase = 0.0f;
        };
        
        std::vector<AnimatedPoint> polePositions;
        std::vector<AnimatedPoint> zeroPositions;
        
        // Interaction state
        int selectedPoleIndex = -1;
        int selectedZeroIndex = -1;
        bool isDragging = false;
        juce::Point<float> dragOffset;
        
        // Energy field visualization
        std::array<std::array<float, 32>, 32> energyField{};
        float fieldPhase = 0.0f;
        
        // Visual parameters
        const float unitCircleRadius = 0.4f;  // Relative to component size
        const float pointRadius = 8.0f;
        const float glowRadius = 20.0f;
        
        // Performance
        juce::Image cachedBackground;
        bool needsBackgroundRedraw = true;
        
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ZPlaneVisualizer)
    };
}
