#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/**
 * Waves PS22-inspired LookAndFeel
 * Professional hardware aesthetic with brushed metal, precise controls
 * Following JUCE best practices for custom LookAndFeel
 */
class WavesPS22LookAndFeel : public juce::LookAndFeel_V4
{
public:
    WavesPS22LookAndFeel();
    ~WavesPS22LookAndFeel() override = default;

    // Color palette - Professional studio hardware
    struct Colors
    {
        static juce::Colour metalLight() { return juce::Colour(0xFFD4D4D4); }
        static juce::Colour metalMid() { return juce::Colour(0xFFB8B8B8); }
        static juce::Colour metalDark() { return juce::Colour(0xFF989898); }
        static juce::Colour blackPanel() { return juce::Colour(0xFF1A1A1A); }
        static juce::Colour ledRed() { return juce::Colour(0xFFFF2020); }
        static juce::Colour ledAmber() { return juce::Colour(0xFFFFB020); }
        static juce::Colour ledGreen() { return juce::Colour(0xFF20FF20); }
        static juce::Colour textWhite() { return juce::Colour(0xFFF0F0F0); }
        static juce::Colour textDark() { return juce::Colour(0xFF303030); }
        static juce::Colour shadowDark() { return juce::Colour(0x80000000); }
        static juce::Colour shadowLight() { return juce::Colour(0x40FFFFFF); }
    };

    // Rotary slider - Precision hardware knob
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                         float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                         juce::Slider& slider) override;

    // Linear slider - Professional fader
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                         float sliderPos, float minSliderPos, float maxSliderPos,
                         const juce::Slider::SliderStyle style, juce::Slider& slider) override;

    // Toggle button - Hardware push button with LED
    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                         bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    // Label - Professional small caps text
    void drawLabel(juce::Graphics& g, juce::Label& label) override;

    // ComboBox - Professional dropdown
    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox& box) override;

    // Fonts
    juce::Font getLabelFont(juce::Label& label) override;
    juce::Font getComboBoxFont(juce::ComboBox& box) override;
    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override;

    // Helper methods
    static void drawBrushedMetal(juce::Graphics& g, juce::Rectangle<float> bounds, bool isVertical = false);
    static void drawInsetPanel(juce::Graphics& g, juce::Rectangle<float> bounds, float depth = 2.0f);
    static void drawLED(juce::Graphics& g, juce::Point<float> center, float radius,
                        juce::Colour color, bool isOn = true);
    static void drawBevel(juce::Graphics& g, juce::Rectangle<float> bounds,
                          bool isRaised = true, float thickness = 1.0f);

private:
    // Cached resources
    juce::Font smallCapsFont;
    juce::Font digitalFont;

    void createFonts();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WavesPS22LookAndFeel)
};

/**
 * Hardware-style X/Y Morph Pad
 * Recessed black panel with precision control
 */
class HardwareMorphPad : public juce::Component
{
public:
    HardwareMorphPad();
    ~HardwareMorphPad() override = default;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

    void setPosition(float x, float y, juce::NotificationType notification = juce::sendNotification);
    juce::Point<float> getPosition() const { return {xPos, yPos}; }

    std::function<void(float, float)> onValueChange;

private:
    std::atomic<float> xPos{0.5f};
    std::atomic<float> yPos{0.5f};
    bool isDragging = false;

    // Efficient repainting
    juce::Rectangle<float> lastIndicatorBounds;
    void repaintIndicator();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HardwareMorphPad)
};

/**
 * Professional LED Button
 * Hardware push button with LED indicator
 */
class LEDButton : public juce::TextButton
{
public:
    LEDButton(const juce::String& text);
    ~LEDButton() override = default;

    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted,
                    bool shouldDrawButtonAsDown) override;

    void setLEDState(bool on) { ledOn = on; repaint(); }
    bool getLEDState() const { return ledOn; }

    void setLEDColor(juce::Colour color) { ledColor = color; repaint(); }

private:
    bool ledOn = false;
    juce::Colour ledColor = WavesPS22LookAndFeel::Colors::ledAmber();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LEDButton)
};

/**
 * Digital Display Component
 * Shows parameter values with 7-segment style
 */
class DigitalDisplay : public juce::Component
{
public:
    DigitalDisplay();
    ~DigitalDisplay() override = default;

    void paint(juce::Graphics& g) override;

    void setValue(float value, const juce::String& unit = "");
    void setText(const juce::String& text);

private:
    juce::String displayText;
    juce::Colour displayColor = WavesPS22LookAndFeel::Colors::ledGreen();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DigitalDisplay)
};