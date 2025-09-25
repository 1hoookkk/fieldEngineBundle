#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../../fx/FieldEngineFXProcessor.h"
#include <array>
#include "FELookAndFeel.h"
#include "CartographyView.h"

class ViralEditor : public juce::AudioProcessorEditor, public juce::Timer
{
public:
    ViralEditor(FieldEngineFXProcessor&);
    ~ViralEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    FieldEngineFXProcessor& audioProcessor;

    FELookAndFeel feLook;
    std::unique_ptr<CartographyView> cartography;

    // High contrast visual elements
    struct SpectrumBar {
        float energy = 0.0f;
        float targetEnergy = 0.0f;
        float morphInfluence = 0.0f;
        juce::Colour color;
    };

    std::array<SpectrumBar, 32> spectrum;
    float morphValue = 0.5f;
    float intensityValue = 0.4f;

    // Essential Z-plane controls
    std::unique_ptr<juce::Slider> morphSlider;
    std::unique_ptr<juce::Slider> intensitySlider;
    std::unique_ptr<juce::Slider> driveSlider;
    std::unique_ptr<juce::Slider> mixSlider;
    std::unique_ptr<juce::Slider> movementRateSlider;

    // FabFilter-style dropdowns
    std::unique_ptr<juce::ComboBox> soloCombo;
    std::unique_ptr<juce::ComboBox> pairCombo;
    std::unique_ptr<juce::ComboBox> syncCombo;

    // Visual state
    float pulsePhase = 0.0f;
    std::array<float, 16> filterResponse;

    void updateVisuals();
    void drawSpectrum(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawFilterResponse(juce::Graphics& g, juce::Rectangle<int> bounds);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ViralEditor)
};