/******************************************************************************
 * File: VintageProLookAndFeel.h
 * Description: Custom LookAndFeel for authentic vintage pro audio aesthetics
 * 
 * SPECTRAL CANVAS PRO - Professional Audio Synthesis Workstation
 * Authentic vintage DAW aesthetics inspired by Cool Edit Pro, Sound Forge, early Pro Tools
 * 
 * Copyright (c) 2025 Spectral Audio Systems
 ******************************************************************************/

#pragma once
#include <JuceHeader.h>

/**
 * @brief Custom LookAndFeel for authentic vintage pro audio workstation aesthetics
 * 
 * Implements the visual styling for SPECTRAL CANVAS PRO with authentic vintage DAW appearance.
 * Features realistic transport controls, professional VU metering, vintage rotary knobs,
 * and classic pro audio color schemes from the golden era of digital audio workstations.
 */
class VintageProLookAndFeel : public juce::LookAndFeel_V4
{
public:
    VintageProLookAndFeel();
    ~VintageProLookAndFeel() override = default;

    //==============================================================================
    // Professional Vintage Color Scheme (inspired by Cool Edit Pro / early Pro Tools)
    
    struct VintageColors
    {
        // Primary interface colors
        static constexpr juce::uint32 backgroundDark = 0xFF2B2B2B;     // Main background
        static constexpr juce::uint32 panelMedium = 0xFF404040;        // Control panels
        static constexpr juce::uint32 panelLight = 0xFF555555;         // Raised elements
        static constexpr juce::uint32 borderDark = 0xFF1A1A1A;         // Recessed borders
        
        // Text and labeling
        static constexpr juce::uint32 textPrimary = 0xFFFFFFFF;        // Primary text
        static constexpr juce::uint32 textSecondary = 0xFFCCCCCC;      // Secondary text
        static constexpr juce::uint32 textDisabled = 0xFF888888;       // Disabled text
        
        // Professional metering colors
        static constexpr juce::uint32 meterGreen = 0xFF00FF00;         // Normal levels
        static constexpr juce::uint32 meterAmber = 0xFFFFB000;         // Warning levels
        static constexpr juce::uint32 meterRed = 0xFFFF0000;           // Peak/error levels
        
        // Accent and highlight colors
        static constexpr juce::uint32 accentBlue = 0xFF0080FF;         // Selected/active
        static constexpr juce::uint32 canvasCyan = 0xFF00FFFF;         // Canvas elements
        static constexpr juce::uint32 ledOff = 0xFF333333;             // LED indicators off
        static constexpr juce::uint32 ledOn = 0xFF00FF00;              // LED indicators on
    };

    //==============================================================================
    // Transport Controls (Professional DAW Style)
    
    void drawButtonBackground(juce::Graphics& g, juce::Button& button, 
                             const juce::Colour& backgroundColour,
                             bool shouldDrawButtonAsHighlighted, 
                             bool shouldDrawButtonAsDown) override;
    
    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                       bool shouldDrawButtonAsHighlighted, 
                       bool shouldDrawButtonAsDown) override;

    //==============================================================================
    // Professional Rotary Knobs (Vintage Hardware Style)
    
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                         float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                         juce::Slider& slider) override;

    //==============================================================================
    // Professional VU Metering (Classic Studio Style)
    
    void drawLevelMeter(juce::Graphics& g, int width, int height, float level) override;
    
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                         float sliderPos, float minSliderPos, float maxSliderPos,
                         juce::Slider::SliderStyle style, juce::Slider& slider) override;

    //==============================================================================
    // Professional Labels and Typography
    
    void drawLabel(juce::Graphics& g, juce::Label& label) override;
    
    juce::Font getLabelFont(juce::Label& label) override;

    //==============================================================================
    // Professional Panel Styling
    
    void fillResizableWindowBackground(juce::Graphics& g, int w, int h,
                                      const juce::BorderSize<int>& border,
                                      juce::ResizableWindow& window) override;

private:
    //==============================================================================
    // Vintage Drawing Utilities
    
    void drawVintageButton(juce::Graphics& g, juce::Rectangle<float> area,
                          bool isPressed, bool isHighlighted, 
                          const juce::String& text, juce::Colour baseColour);
    
    void drawVintageKnob(juce::Graphics& g, juce::Rectangle<float> area,
                        float angle, bool isEnabled);
    
    void drawVintageMeter(juce::Graphics& g, juce::Rectangle<float> area,
                         float level, bool isVertical);
    
    void drawVintagePanel(juce::Graphics& g, juce::Rectangle<float> area,
                         bool isRecessed = false);
    
    void drawVintageBorder(juce::Graphics& g, juce::Rectangle<float> area,
                          int thickness = 1, bool isInset = true);

    //==============================================================================
    // Professional Typography
    
    juce::Font getVintageFont(float height, bool isBold = false) const;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VintageProLookAndFeel)
};