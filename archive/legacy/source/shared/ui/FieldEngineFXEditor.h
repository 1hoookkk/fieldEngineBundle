#pragma once

#include <JuceHeader.h>
#include "ZPlaneGalaxy.h"
#include "BiomechanicalKnob.h"
#include "PresetNebula.h"
#include "EnergyFlowVisualizer.h"
#include "ModulationMatrix.h"

namespace FieldEngineFX {

// Forward declarations
class FieldEngineFXProcessor;

/**
 * Main plugin editor - An alien control interface for the FieldEngineFX
 * Integrates all xenomorphic UI components into a cohesive experience
 */
class FieldEngineFXEditor : public juce::AudioProcessorEditor,
                            public juce::Timer,
                            private juce::AudioProcessorValueTreeState::Listener {
public:
    FieldEngineFXEditor(FieldEngineFXProcessor& processor, 
                        juce::AudioProcessorValueTreeState& vts);
    ~FieldEngineFXEditor() override;

    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    // Editor state
    void setInterfaceMode(int mode); // 0: Galaxy, 1: Nebula, 2: Matrix
    int getInterfaceMode() const { return currentMode; }

private:
    // References
    FieldEngineFXProcessor& audioProcessor;
    juce::AudioProcessorValueTreeState& valueTreeState;
    
    // Main components
    std::unique_ptr<UI::ZPlaneGalaxy> zplaneGalaxy;
    std::unique_ptr<UI::PresetNebula> presetNebula;
    std::unique_ptr<UI::EnergyFlowVisualizer> energyFlow;
    std::unique_ptr<UI::ModulationMatrix> modMatrix;
    
    // Control surfaces
    std::unique_ptr<UI::ResonantBiomechanicalKnob> cutoffKnob;
    std::unique_ptr<UI::ResonantBiomechanicalKnob> resonanceKnob;
    std::unique_ptr<UI::BiomechanicalKnob> morphKnob;
    std::unique_ptr<UI::BiomechanicalKnob> driveKnob;
    std::unique_ptr<UI::BiomechanicalKnob> mixKnob;
    
    // Navigation
    std::unique_ptr<juce::DrawableButton> galaxyModeButton;
    std::unique_ptr<juce::DrawableButton> nebulaModeButton;
    std::unique_ptr<juce::DrawableButton> matrixModeButton;
    
    // State
    int currentMode = 0;
    float interfaceOpacity = 1.0f;
    bool isInitialized = false;
    
    // Background rendering
    struct BackgroundRenderer {
        void render(juce::Graphics& g, juce::Rectangle<int> bounds, float time);
        
    private:
        void drawStarfield(juce::Graphics& g, juce::Rectangle<int> bounds, float time);
        void drawNebulaClouds(juce::Graphics& g, juce::Rectangle<int> bounds, float time);
        void drawEnergyGrid(juce::Graphics& g, juce::Rectangle<int> bounds, float time);
    } backgroundRenderer;
    
    // Layout helpers
    struct LayoutGrid {
        juce::Rectangle<int> mainDisplay;
        juce::Rectangle<int> controlPanel;
        juce::Rectangle<int> modulationSection;
        juce::Rectangle<int> presetSection;
        juce::Rectangle<int> navigationBar;
        
        void calculate(juce::Rectangle<int> totalBounds);
    } layout;
    
    // Animation
    void timerCallback() override;
    float animationTime = 0.0f;
    
    // Parameter handling
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void attachParameters();
    void detachParameters();
    
    // UI Setup
    void setupComponents();
    void setupKnobs();
    void setupNavigation();
    void applyAlienStyling();
    
    // Mode transitions
    void transitionToMode(int newMode);
    void animateModeTransition(float progress);
    
    // Tooltips and feedback
    std::unique_ptr<juce::TooltipWindow> tooltipWindow;
    void showContextualHelp(juce::Component* component);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FieldEngineFXEditor)
};

/**
 * Specialized label with alien glyph rendering
 */
class GlyphLabel : public juce::Component {
public:
    GlyphLabel(const juce::String& text = "");
    
    void setText(const juce::String& newText);
    void setGlyphComplexity(int complexity) { glyphComplexity = complexity; }
    void setGlowIntensity(float intensity) { glowIntensity = intensity; }
    
    void paint(juce::Graphics& g) override;
    
private:
    juce::String text;
    int glyphComplexity = 5;
    float glowIntensity = 0.8f;
    juce::Path glyphPath;
    
    void generateGlyph();
};



} // namespace FieldEngineFX
