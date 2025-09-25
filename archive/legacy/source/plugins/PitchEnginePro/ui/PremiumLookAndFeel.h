#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>

namespace PitchEngineUI
{
    // Premium color palette for professional appearance
    namespace Colors
    {
        // Base colors
        const juce::Colour background      { 0xFF0A0B0D };  // Deep black
        const juce::Colour panel           { 0xFF141619 };  // Dark grey panel
        const juce::Colour panelHighlight  { 0xFF1A1D21 };  // Lighter panel
        const juce::Colour border          { 0xFF2A2E34 };  // Subtle border
        const juce::Colour borderLight     { 0xFF3A3F47 };  // Lighter border

        // Text hierarchy
        const juce::Colour textPrimary     { 0xFFFAFBFC };  // Pure white
        const juce::Colour textSecondary   { 0xFFB8BCC4 };  // Muted text
        const juce::Colour textDisabled    { 0xFF5A5E66 };  // Disabled text

        // Accent colors (premium gold/cyan theme)
        const juce::Colour accentPrimary   { 0xFFF4C447 };  // Premium gold
        const juce::Colour accentSecondary { 0xFF47E4F4 };  // Electric cyan
        const juce::Colour accentGlow      { 0xFF67E0C1 };  // Mint glow
        const juce::Colour accentWarm      { 0xFFFF9547 };  // Warm orange

        // Status colors
        const juce::Colour statusActive    { 0xFF47F473 };  // Active green
        const juce::Colour statusWarning   { 0xFFF4E447 };  // Warning yellow
        const juce::Colour statusDanger    { 0xFFF44747 };  // Danger red

        // Meter colors
        const juce::Colour meterLow        { 0xFF47E4F4 };  // Cyan
        const juce::Colour meterMid        { 0xFF47F473 };  // Green
        const juce::Colour meterHigh       { 0xFFF4E447 };  // Yellow
        const juce::Colour meterPeak       { 0xFFF44747 };  // Red
    }

    class PremiumLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        PremiumLookAndFeel();
        ~PremiumLookAndFeel() override = default;

        // Premium rotary slider with hardware-inspired design
        void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                             float sliderPosProportional, float rotaryStartAngle,
                             float rotaryEndAngle, juce::Slider& slider) override;

        // Professional combo box
        void drawComboBox(juce::Graphics& g, int width, int height,
                         bool isButtonDown, int buttonX, int buttonY,
                         int buttonW, int buttonH, juce::ComboBox& box) override;

        // Premium toggle button
        void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                             bool shouldDrawButtonAsHighlighted,
                             bool shouldDrawButtonAsDown) override;

        // Professional popup menu
        void drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                              bool isSeparator, bool isActive, bool isHighlighted,
                              bool isTicked, bool hasSubMenu, const juce::String& text,
                              const juce::String& shortcutKeyText,
                              const juce::Drawable* icon, const juce::Colour* textColour) override;

        // Custom slider text box
        juce::Label* createSliderTextBox(juce::Slider& slider) override;

    private:
        // Helper methods for drawing
        void drawHardwareBezel(juce::Graphics& g, juce::Rectangle<float> bounds,
                               bool isPressed = false);
        void drawLEDIndicator(juce::Graphics& g, juce::Point<float> center,
                              float radius, juce::Colour color, bool isOn);
        void drawMetallicKnob(juce::Graphics& g, juce::Rectangle<float> bounds,
                              float angle, juce::Colour accentColor);

        // Cached gradients for performance
        void createGradients();
        std::unique_ptr<juce::ColourGradient> metalGradient;
        std::unique_ptr<juce::ColourGradient> glowGradient;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PremiumLookAndFeel)
    };
}