#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>

namespace MorphEngineUI
{
    // Premium color scheme for professional hardware aesthetic
    namespace Colors
    {
        // Dark sophisticated theme with gold accents
        const juce::Colour background      { 0xFF0D0E11 };  // Rich black
        const juce::Colour panel           { 0xFF161820 };  // Dark gunmetal
        const juce::Colour panelRaised     { 0xFF1E212A };  // Raised surface
        const juce::Colour panelInset      { 0xFF0A0B0E };  // Inset shadow

        // Metal and hardware textures
        const juce::Colour brushedMetal    { 0xFF2A2E38 };  // Brushed aluminum
        const juce::Colour chrome          { 0xFF3F4552 };  // Chrome accent
        const juce::Colour darkChrome      { 0xFF252830 };  // Dark chrome

        // Premium accent colors
        const juce::Colour goldPrimary     { 0xFFD4AF37 };  // Luxury gold
        const juce::Colour goldBright      { 0xFFF4E4B5 };  // Bright gold highlight
        const juce::Colour goldDark        { 0xFFB8962F };  // Deep gold shadow
        const juce::Colour amber           { 0xFFFF9F40 };  // Warm amber LED

        // Elegant blue accent
        const juce::Colour bluePrimary     { 0xFF4A90E2 };  // Premium blue
        const juce::Colour blueGlow        { 0xFF6FA9F3 };  // Blue LED glow
        const juce::Colour blueDark        { 0xFF2E5C8A };  // Deep blue

        // Text hierarchy
        const juce::Colour textPrimary     { 0xFFF0F0F2 };  // Clean white
        const juce::Colour textSecondary   { 0xFFB8BCC8 };  // Soft grey
        const juce::Colour textMuted       { 0xFF707580 };  // Muted labels
        const juce::Colour textGold        { 0xFFD4AF37 };  // Gold highlight text

        // Meter and visualization
        const juce::Colour meterGreen      { 0xFF4AE26F };  // Signal present
        const juce::Colour meterYellow     { 0xFFE2D44A };  // Peak warning
        const juce::Colour meterRed        { 0xFFE24A4A };  // Clip/overload

        // Interactive states
        const juce::Colour hoverGlow       { 0x30D4AF37 };  // Gold hover
        const juce::Colour activeGlow      { 0x60D4AF37 };  // Gold active
        const juce::Colour focusRing       { 0xFFD4AF37 };  // Gold focus
    }

    class MorphEngineLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        MorphEngineLookAndFeel();
        ~MorphEngineLookAndFeel() override = default;

        // Premium rotary slider with hardware-inspired bezel
        void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                             float sliderPosProportional, float rotaryStartAngle,
                             float rotaryEndAngle, juce::Slider& slider) override;

        // Elegant linear slider with LED-style position indicator
        void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                             float sliderPos, float minSliderPos, float maxSliderPos,
                             const juce::Slider::SliderStyle style, juce::Slider& slider) override;

        // Professional combo box with hardware styling
        void drawComboBox(juce::Graphics& g, int width, int height,
                         bool isButtonDown, int buttonX, int buttonY,
                         int buttonW, int buttonH, juce::ComboBox& box) override;

        // Premium button design
        void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                 const juce::Colour& backgroundColour,
                                 bool shouldDrawButtonAsHighlighted,
                                 bool shouldDrawButtonAsDown) override;

        // Custom popup menu
        void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override;

        // Label with premium typography
        juce::Font getLabelFont(juce::Label& label) override;

    private:
        // Helper drawing methods
        void drawMetallicBezel(juce::Graphics& g, juce::Rectangle<float> bounds);
        void drawLEDIndicator(juce::Graphics& g, juce::Point<float> center,
                             float radius, juce::Colour color, float intensity);
        void drawKnobTick(juce::Graphics& g, juce::Point<float> center,
                         float radius, float angle, bool isMain = false);
        void drawInsetShadow(juce::Graphics& g, juce::Rectangle<float> bounds,
                           float depth = 3.0f);
        void drawRaisedSurface(juce::Graphics& g, juce::Rectangle<float> bounds,
                              float height = 2.0f);

        // Typography
        juce::Font getUIFont(float height, bool bold = false);

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MorphEngineLookAndFeel)
    };

    // Custom knob component for premium control
    class HardwareKnob : public juce::Slider
    {
    public:
        HardwareKnob()
        {
            setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        }

        void paint(juce::Graphics& g) override;
        void mouseEnter(const juce::MouseEvent& event) override;
        void mouseExit(const juce::MouseEvent& event) override;

    private:
        bool isHovering = false;
        float hoverAnimation = 0.0f;
    };
}