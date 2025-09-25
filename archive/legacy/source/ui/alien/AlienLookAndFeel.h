#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "EMUColorPalette.h"
#include "AlienGlyphs.h"

namespace AlienUI
{
    class AlienLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        AlienLookAndFeel();
        ~AlienLookAndFeel() override = default;
        
        // Slider customization
        void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                             float sliderPosProportional, float rotaryStartAngle,
                             float rotaryEndAngle, juce::Slider& slider) override;
                             
        void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                             float sliderPos, float minSliderPos, float maxSliderPos,
                             const juce::Slider::SliderStyle style, juce::Slider& slider) override;
        
        // Button customization
        void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                 const juce::Colour& backgroundColour,
                                 bool shouldDrawButtonAsHighlighted,
                                 bool shouldDrawButtonAsDown) override;
                                 
        void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                           bool shouldDrawButtonAsHighlighted,
                           bool shouldDrawButtonAsDown) override;
        
        // Label customization
        juce::Font getLabelFont(juce::Label& label) override;
        void drawLabel(juce::Graphics& g, juce::Label& label) override;
        
        // ComboBox customization
        void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                         int buttonX, int buttonY, int buttonW, int buttonH,
                         juce::ComboBox& box) override;
                         
        // Popup menu customization
        void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override;
        
        void drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                              bool isSeparator, bool isActive, bool isHighlighted,
                              bool isTicked, bool hasSubMenu,
                              const juce::String& text,
                              const juce::String& shortcutKeyText,
                              const juce::Drawable* icon,
                              const juce::Colour* textColour) override;
        
        // ScrollBar customization
        void drawScrollbar(juce::Graphics& g, juce::ScrollBar& scrollbar,
                          int x, int y, int width, int height,
                          bool isScrollbarVertical, int thumbStartPosition,
                          int thumbSize, bool isMouseOver, bool isMouseDown) override;
        
        // Helper functions
        void drawAlienKnob(juce::Graphics& g, juce::Rectangle<float> bounds,
                          float value, const juce::String& label,
                          bool isMouseOver, bool isMouseDown);
                          
        void drawEnergyRing(juce::Graphics& g, juce::Rectangle<float> bounds,
                           float energy, juce::Colour baseColor);
                           
        void drawGlowEffect(juce::Graphics& g, juce::Rectangle<float> bounds,
                           juce::Colour glowColor, float intensity);
        
    private:
        // Animation state
        std::unordered_map<juce::Component*, float> glowIntensities;
        std::unordered_map<juce::Component*, float> energyLevels;
        
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AlienLookAndFeel)
    };
}
