#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>

namespace MorphEngineUI
{
    // Authentic EMU Xtreme Lead color palette
    namespace XtremeColors
    {
        // Hardware chassis colors
        const juce::Colour chassisBlack    { 0xFF0A0A0A };  // EMU black chassis
        const juce::Colour chassisGrey     { 0xFF1A1A1A };  // Dark grey plastic
        const juce::Colour metalPanel      { 0xFF2D2D2D };  // Brushed metal panel
        const juce::Colour metalHighlight  { 0xFF3A3A3A };  // Metal edge highlight

        // Signature EMU display colors (orange/amber LCD)
        const juce::Colour lcdBackground   { 0xFF1A0800 };  // Dark LCD background
        const juce::Colour lcdAmber        { 0xFFFF6600 };  // Classic amber/orange LCD
        const juce::Colour lcdAmberBright  { 0xFFFF8800 };  // Bright amber
        const juce::Colour lcdAmberDim     { 0xFFCC5500 };  // Dimmed amber
        const juce::Colour lcdPixelGrid    { 0x20FF6600 };  // LCD pixel grid effect

        // EMU blue LED accent colors
        const juce::Colour ledBlue         { 0xFF0080FF };  // EMU signature blue LED
        const juce::Colour ledBlueBright   { 0xFF00A0FF };  // Bright blue LED
        const juce::Colour ledBlueDim      { 0xFF0060CC };  // Dimmed blue
        const juce::Colour ledBlueGlow     { 0x400080FF };  // Blue LED glow

        // Control surface colors
        const juce::Colour knobPlastic     { 0xFF1E1E1E };  // Black plastic knob
        const juce::Colour knobRubber      { 0xFF282828 };  // Rubberized knob
        const juce::Colour knobIndicator   { 0xFFFFFFFF };  // White position marker
        const juce::Colour buttonInset     { 0xFF0D0D0D };  // Button inset shadow

        // Text and labels
        const juce::Colour textSilkscreen  { 0xFFE8E8E8 };  // White silkscreen text
        const juce::Colour textMuted       { 0xFF808080 };  // Grey secondary text
        const juce::Colour textLCD         { 0xFFFF7700 };  // LCD text color

        // Function key colors (EMU style)
        const juce::Colour functionRed     { 0xFFFF2020 };  // Function button red
        const juce::Colour functionYellow  { 0xFFFFCC00 };  // Function button yellow
        const juce::Colour functionGreen   { 0xFF00FF40 };  // Function button green

        // VU meter colors
        const juce::Colour meterGreen      { 0xFF00FF00 };  // -inf to -12dB
        const juce::Colour meterYellow     { 0xFFFFFF00 };  // -12dB to -3dB
        const juce::Colour meterRed        { 0xFFFF0000 };  // -3dB to 0dB
    }

    class XtremeLeadLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        XtremeLeadLookAndFeel();
        ~XtremeLeadLookAndFeel() override = default;

        // EMU-style rotary encoder with LED ring
        void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                             float sliderPosProportional, float rotaryStartAngle,
                             float rotaryEndAngle, juce::Slider& slider) override;

        // EMU-style fader
        void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                             float sliderPos, float minSliderPos, float maxSliderPos,
                             const juce::Slider::SliderStyle style, juce::Slider& slider) override;

        // EMU-style button with LED
        void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                 const juce::Colour& backgroundColour,
                                 bool shouldDrawButtonAsHighlighted,
                                 bool shouldDrawButtonAsDown) override;

        // EMU LCD-style combo box
        void drawComboBox(juce::Graphics& g, int width, int height,
                         bool isButtonDown, int buttonX, int buttonY,
                         int buttonW, int buttonH, juce::ComboBox& box) override;

        // Label with EMU typography
        juce::Font getLabelFont(juce::Label& label) override;
        void drawLabel(juce::Graphics& g, juce::Label& label) override;

        // EMU-style popup menu
        void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override;
        void drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                              bool isSeparator, bool isActive, bool isHighlighted,
                              bool isTicked, bool hasSubMenu,
                              const juce::String& text, const juce::String& shortcutKeyText,
                              const juce::Drawable* icon, const juce::Colour* textColour) override;

    private:
        // Helper drawing methods
        void drawChassisBezel(juce::Graphics& g, juce::Rectangle<float> bounds);
        void drawLEDRing(juce::Graphics& g, juce::Rectangle<float> bounds,
                        float value, int numLEDs = 16);
        void drawLCDDisplay(juce::Graphics& g, juce::Rectangle<float> bounds,
                           const juce::String& text, bool withPixelGrid = true);
        void drawEMUKnob(juce::Graphics& g, juce::Rectangle<float> bounds,
                        float angle, bool isEndlessEncoder = false);
        void drawRubberButton(juce::Graphics& g, juce::Rectangle<float> bounds,
                             bool isPressed, bool hasLED = false, bool ledOn = false);

        // Typography
        juce::Font getEMUFont(float height, bool isLCD = false);

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(XtremeLeadLookAndFeel)
    };

    // Custom LCD display component
    class XtremeLCDDisplay : public juce::Component
    {
    public:
        XtremeLCDDisplay()
        {
            setOpaque(true);
        }

        void setText(const juce::String& newText, bool centerAlign = true)
        {
            displayText = newText;
            centered = centerAlign;
            repaint();
        }

        void paint(juce::Graphics& g) override;

    private:
        juce::String displayText;
        bool centered = true;
        void drawPixelGrid(juce::Graphics& g);
    };

    // Custom EMU-style encoder with LED ring
    class XtremeEncoder : public juce::Slider
    {
    public:
        XtremeEncoder()
        {
            setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            setRotaryParameters(0, juce::MathConstants<float>::twoPi, false);
        }

        void setLEDCount(int count) { ledCount = count; }
        void paint(juce::Graphics& g) override;

    private:
        int ledCount = 16;
        bool isHovering = false;
    };
}