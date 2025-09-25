#pragma once

#include <JuceHeader.h>
#include "Core/Commands.h"
#include "../CanvasComponent.h"

// Forward declarations
class ARTEFACTAudioProcessor;

/**
 * PaintControlPanel - Modern vibrant controls for SpectralCanvas
 * 
 * Provides clean, satisfying controls for:
 * - Brush selection and parameters
 * - Canvas settings (frequency range, clear)
 * - Master gain and paint mode
 * - Spectral masking with drum samples
 */
class PaintControlPanel : public juce::Component,
                         public juce::Button::Listener,
                         public juce::Slider::Listener,
                         public juce::ComboBox::Listener
{
public:
    //==============================================================================
    // Main Interface
    
    explicit PaintControlPanel(ARTEFACTAudioProcessor& processorToUse);
    ~PaintControlPanel() override;
    
    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    // Control callbacks
    void buttonClicked(juce::Button* button) override;
    void sliderValueChanged(juce::Slider* slider) override;
    void comboBoxChanged(juce::ComboBox* comboBox) override;
    
    // Canvas integration
    void setCanvasComponent(CanvasComponent* canvas) { canvasComponent = canvas; }
    
private:
    //==============================================================================
    // SpectralCanvas Modern Colors (Early 2000s Pro Audio Inspired)
    
    static const juce::Colour PANEL_BACKGROUND;
    static const juce::Colour PRIMARY_BLUE;
    static const juce::Colour SECONDARY_BLUE;
    static const juce::Colour TEXT_DARK;
    static const juce::Colour ACCENT_PURPLE;
    
    //==============================================================================
    // Control Groups
    
    // Brush Controls
    juce::Label brushLabel {"brushLabel", "BRUSH"};
    juce::TextButton sineBrushButton {"SINE"};
    juce::TextButton harmonicBrushButton {"HARMONIC"};
    juce::TextButton noiseBrushButton {"NOISE"};
    juce::TextButton sampleBrushButton {"SAMPLE"};
    juce::TextButton granularBrushButton {"GRANULAR"};
    juce::TextButton cdpMorphButton {"CDP-MORPH"};
    
    // Brush Parameters
    juce::Label brushSizeLabel {"brushSizeLabel", "SIZE"};
    juce::Slider brushSizeSlider;
    juce::Label brushPressureLabel {"brushPressureLabel", "PRESSURE"};
    juce::Slider brushPressureSlider;
    
    // Sample Brush Controls
    juce::Label sampleSlotLabel {"sampleSlotLabel", "SLOT"};
    juce::Slider sampleSlotSlider;
    
    // Canvas Controls
    juce::Label canvasLabel {"canvasLabel", "CANVAS"};
    juce::TextButton clearCanvasButton {"CLEAR ALL"};
    juce::TextButton resetViewButton {"RESET VIEW"};
    
    // Frequency Range
    juce::Label freqRangeLabel {"freqRangeLabel", "FREQ RANGE"};
    juce::Slider minFreqSlider;
    juce::Slider maxFreqSlider;
    juce::Label minFreqValueLabel {"minFreqValue", "80Hz"};
    juce::Label maxFreqValueLabel {"maxFreqValue", "8kHz"};
    
    // Master Controls
    juce::Label masterLabel {"masterLabel", "MASTER"};
    juce::Slider masterGainSlider;
    juce::Label masterGainValueLabel {"masterGainValue", "70%"};
    juce::TextButton paintActiveButton {"PAINT: OFF"};
    
    // Mode Controls
    juce::Label modeLabel {"modeLabel", "MODE"};
    juce::TextButton canvasModeButton {"CANVAS"};
    juce::TextButton forgeModeButton {"FORGE"};
    juce::TextButton hybridModeButton {"HYBRID"};
    
    // Spectral Masking Controls (MetaSynth-style)
    juce::Label spectralMaskLabel {"spectralMaskLabel", "SPECTRAL MASK"};
    juce::TextButton spectralMaskEnableButton {"MASK: OFF"};
    juce::Label maskSourceLabel {"maskSourceLabel", "SOURCE"};
    juce::Slider maskSourceSlider;
    juce::Label maskTypeLabel {"maskTypeLabel", "TYPE"};
    juce::ComboBox maskTypeComboBox;
    juce::Label maskStrengthLabel {"maskStrengthLabel", "STRENGTH"};
    juce::Slider maskStrengthSlider;
    juce::Label timeStretchLabel {"timeStretchLabel", "TIME"};
    juce::Slider timeStretchSlider;
    
    //==============================================================================
    // State Management
    
    ARTEFACTAudioProcessor& processor;
    CanvasComponent* canvasComponent = nullptr;
    
    int currentBrushType = 0;  // 0 = SineBrush, 1 = HarmonicBrush, etc.
    bool isPaintActive = false;
    bool isSpectralMaskEnabled = false;
    int currentMaskSource = 0;  // ForgeVoice slot index (0-7)
    
    //==============================================================================
    // Helper Methods
    
    void setupControls();
    void updateBrushButtons();
    void updateModeButtons();
    void updateSpectralMaskControls();
    void sendPaintCommand(PaintCommandID commandID, float value1 = 0.0f, float value2 = 0.0f);
    void sendForgeCommand(ForgeCommandID commandID, float value = 0.0f);
    
    // Modern vibrant drawing
    void drawModernSection(juce::Graphics& g, juce::Rectangle<int> area, 
                          const juce::String& title, juce::Colour borderColor);
    juce::Font createModernFont(float size) const;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PaintControlPanel)
};