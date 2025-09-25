#include "XtremeLeadLookAndFeel.h"

namespace MorphEngineUI
{
    XtremeLeadLookAndFeel::XtremeLeadLookAndFeel()
    {
        // Set default colors
        setColour(juce::ResizableWindow::backgroundColourId, XtremeColors::chassisBlack);
        setColour(juce::Slider::backgroundColourId, XtremeColors::metalPanel);
        setColour(juce::Slider::trackColourId, XtremeColors::ledBlueDim);
        setColour(juce::Slider::thumbColourId, XtremeColors::knobPlastic);
        setColour(juce::TextButton::buttonColourId, XtremeColors::chassisGrey);
        setColour(juce::TextButton::buttonOnColourId, XtremeColors::ledBlue);
        setColour(juce::TextButton::textColourOffId, XtremeColors::textSilkscreen);
        setColour(juce::TextButton::textColourOnId, XtremeColors::lcdAmber);
        setColour(juce::Label::textColourId, XtremeColors::textSilkscreen);
        setColour(juce::ComboBox::backgroundColourId, XtremeColors::lcdBackground);
        setColour(juce::ComboBox::textColourId, XtremeColors::lcdAmber);
        setColour(juce::PopupMenu::backgroundColourId, XtremeColors::chassisGrey);
        setColour(juce::PopupMenu::textColourId, XtremeColors::textSilkscreen);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, XtremeColors::ledBlue);
    }

    void XtremeLeadLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                                 float sliderPosProportional, float rotaryStartAngle,
                                                 float rotaryEndAngle, juce::Slider& slider)
    {
        auto bounds = juce::Rectangle<float>(x, y, width, height).reduced(4.0f);
        auto centre = bounds.getCentre();
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;

        // Draw chassis bezel
        drawChassisBezel(g, bounds);

        // Draw LED ring around knob
        drawLEDRing(g, bounds.reduced(radius * 0.15f), sliderPosProportional, 16);

        // Draw main knob
        auto knobRadius = radius * 0.6f;
        auto knobBounds = juce::Rectangle<float>(knobRadius * 2, knobRadius * 2).withCentre(centre);

        // Knob shadow
        g.setColour(XtremeColors::buttonInset);
        g.fillEllipse(knobBounds.translated(1, 1));

        // Main knob body
        g.setColour(XtremeColors::knobPlastic);
        g.fillEllipse(knobBounds);

        // Knob edge highlight
        g.setColour(XtremeColors::metalHighlight);
        g.drawEllipse(knobBounds.reduced(1), 1.0f);

        // Position indicator
        auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        auto indicatorLength = knobRadius * 0.7f;
        auto indicatorStart = centre.getPointOnCircumference(knobRadius * 0.3f, angle);
        auto indicatorEnd = centre.getPointOnCircumference(indicatorLength, angle);

        g.setColour(XtremeColors::knobIndicator);
        g.drawLine(indicatorStart.x, indicatorStart.y, indicatorEnd.x, indicatorEnd.y, 2.0f);

        // Center dot
        g.setColour(XtremeColors::ledBlue);
        g.fillEllipse(juce::Rectangle<float>(4, 4).withCentre(centre));
    }

    void XtremeLeadLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                                 float sliderPos, float minSliderPos, float maxSliderPos,
                                                 const juce::Slider::SliderStyle style, juce::Slider& slider)
    {
        auto bounds = juce::Rectangle<float>(x, y, width, height);

        // Draw track background
        auto trackBounds = (style == juce::Slider::LinearHorizontal)
            ? bounds.reduced(0, height * 0.4f)
            : bounds.reduced(width * 0.4f, 0);

        g.setColour(XtremeColors::buttonInset);
        g.fillRoundedRectangle(trackBounds, 2.0f);

        // Draw active track
        auto activeBounds = trackBounds;
        if (style == juce::Slider::LinearHorizontal)
        {
            activeBounds = activeBounds.withWidth((sliderPos - minSliderPos) / (maxSliderPos - minSliderPos) * trackBounds.getWidth());
        }
        else
        {
            float proportion = 1.0f - (sliderPos - minSliderPos) / (maxSliderPos - minSliderPos);
            activeBounds = activeBounds.withTop(activeBounds.getY() + proportion * trackBounds.getHeight());
        }

        g.setColour(XtremeColors::ledBlue);
        g.fillRoundedRectangle(activeBounds, 2.0f);

        // Draw thumb
        auto thumbSize = (style == juce::Slider::LinearHorizontal) ? height * 0.8f : width * 0.8f;
        auto thumbBounds = juce::Rectangle<float>(thumbSize, thumbSize).withCentre(juce::Point<float>(sliderPos, bounds.getCentreY()));

        if (style == juce::Slider::LinearVertical)
            thumbBounds = juce::Rectangle<float>(thumbSize, thumbSize).withCentre(juce::Point<float>(bounds.getCentreX(), sliderPos));

        g.setColour(XtremeColors::metalPanel);
        g.fillRoundedRectangle(thumbBounds, 3.0f);

        g.setColour(XtremeColors::metalHighlight);
        g.drawRoundedRectangle(thumbBounds, 3.0f, 1.0f);

        // Center line on thumb
        g.setColour(XtremeColors::knobIndicator);
        if (style == juce::Slider::LinearHorizontal)
            g.fillRect(thumbBounds.getCentreX() - 1, thumbBounds.getY() + 3, 2.0f, thumbBounds.getHeight() - 6);
        else
            g.fillRect(thumbBounds.getX() + 3, thumbBounds.getCentreY() - 1, thumbBounds.getWidth() - 6, 2.0f);
    }

    void XtremeLeadLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                                     const juce::Colour& backgroundColour,
                                                     bool shouldDrawButtonAsHighlighted,
                                                     bool shouldDrawButtonAsDown)
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(2);

        // Button bezel
        if (shouldDrawButtonAsDown)
        {
            g.setColour(XtremeColors::buttonInset);
            g.fillRoundedRectangle(bounds, 3.0f);
        }
        else
        {
            // Raised button
            g.setColour(XtremeColors::metalHighlight);
            g.fillRoundedRectangle(bounds.translated(0, 1), 3.0f);

            g.setColour(XtremeColors::chassisGrey);
            g.fillRoundedRectangle(bounds, 3.0f);
        }

        // LED indicator
        if (button.getToggleState())
        {
            auto ledBounds = juce::Rectangle<float>(8, 8).withPosition(bounds.getRight() - 12, bounds.getY() + 4);

            // LED glow
            g.setColour(XtremeColors::ledBlueGlow);
            g.fillEllipse(ledBounds.expanded(3));

            // LED
            g.setColour(XtremeColors::ledBlueBright);
            g.fillEllipse(ledBounds);
        }

        // Button highlight
        if (shouldDrawButtonAsHighlighted && !shouldDrawButtonAsDown)
        {
            g.setColour(XtremeColors::ledBlueGlow);
            g.drawRoundedRectangle(bounds, 3.0f, 2.0f);
        }
    }

    void XtremeLeadLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height,
                                             bool isButtonDown, int buttonX, int buttonY,
                                             int buttonW, int buttonH, juce::ComboBox& box)
    {
        auto bounds = box.getLocalBounds().toFloat();

        // LCD-style background
        g.setColour(XtremeColors::lcdBackground);
        g.fillRoundedRectangle(bounds, 2.0f);

        // LCD border
        g.setColour(XtremeColors::metalPanel);
        g.drawRoundedRectangle(bounds, 2.0f, 1.0f);

        // Arrow button area
        auto arrowBounds = juce::Rectangle<float>(buttonX, buttonY, buttonW, buttonH);

        // Draw arrow
        juce::Path arrow;
        arrow.addTriangle(arrowBounds.getCentreX() - 4, arrowBounds.getCentreY() - 2,
                         arrowBounds.getCentreX() + 4, arrowBounds.getCentreY() - 2,
                         arrowBounds.getCentreX(), arrowBounds.getCentreY() + 3);

        g.setColour(isButtonDown ? XtremeColors::lcdAmberBright : XtremeColors::lcdAmber);
        g.fillPath(arrow);
    }

    juce::Font XtremeLeadLookAndFeel::getLabelFont(juce::Label& label)
    {
        return getEMUFont(label.getFont().getHeight(), false);
    }

    void XtremeLeadLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& label)
    {
        auto bounds = label.getLocalBounds();

        g.setColour(label.findColour(juce::Label::backgroundColourId));
        g.fillRect(bounds);

        if (!label.isBeingEdited())
        {
            auto alpha = label.isEnabled() ? 1.0f : 0.5f;
            g.setColour(label.findColour(juce::Label::textColourId).withMultipliedAlpha(alpha));
            g.setFont(getLabelFont(label));
            g.drawFittedText(label.getText(), bounds, label.getJustificationType(),
                           juce::jmax(1, (int)(bounds.getHeight() / label.getFont().getHeight())),
                           label.getMinimumHorizontalScale());
        }
    }

    void XtremeLeadLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height)
    {
        auto bounds = juce::Rectangle<float>(0, 0, width, height);

        // EMU-style popup background
        g.setColour(XtremeColors::chassisGrey);
        g.fillRect(bounds);

        // Border
        g.setColour(XtremeColors::metalHighlight);
        g.drawRect(bounds, 1.0f);
    }

    void XtremeLeadLookAndFeel::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                                                  bool isSeparator, bool isActive, bool isHighlighted,
                                                  bool isTicked, bool hasSubMenu,
                                                  const juce::String& text, const juce::String& shortcutKeyText,
                                                  const juce::Drawable* icon, const juce::Colour* textColour)
    {
        if (isSeparator)
        {
            auto r = area.reduced(5, 0).removeFromTop(1);
            g.setColour(XtremeColors::metalHighlight);
            g.fillRect(r);
            return;
        }

        auto textColor = XtremeColors::textSilkscreen;

        if (isHighlighted && isActive)
        {
            g.setColour(XtremeColors::ledBlue);
            g.fillRect(area);
            textColor = XtremeColors::textSilkscreen;
        }
        else if (!isActive)
        {
            textColor = XtremeColors::textMuted;
        }

        g.setColour(textColor);
        g.setFont(getEMUFont(14.0f));

        auto r = area.reduced(2);

        if (isTicked)
        {
            g.setColour(XtremeColors::ledBlueBright);
            g.fillEllipse(r.removeFromLeft(12).reduced(2).toFloat());
        }

        r.removeFromLeft(8);
        g.drawFittedText(text, r, juce::Justification::centredLeft, 1);
    }

    // Helper methods implementation
    void XtremeLeadLookAndFeel::drawChassisBezel(juce::Graphics& g, juce::Rectangle<float> bounds)
    {
        // Outer bezel shadow
        g.setColour(XtremeColors::buttonInset);
        g.drawRoundedRectangle(bounds.expanded(1), 4.0f, 2.0f);

        // Main bezel
        g.setColour(XtremeColors::metalPanel);
        g.drawRoundedRectangle(bounds, 3.0f, 1.5f);
    }

    void XtremeLeadLookAndFeel::drawLEDRing(juce::Graphics& g, juce::Rectangle<float> bounds,
                                           float value, int numLEDs)
    {
        auto centre = bounds.getCentre();
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;

        for (int i = 0; i < numLEDs; ++i)
        {
            float ledPosition = (float)i / (float)numLEDs;
            float angle = -2.35f + ledPosition * 4.7f; // -135 to +135 degrees

            auto ledCentre = centre.getPointOnCircumference(radius, angle);
            auto ledBounds = juce::Rectangle<float>(6, 6).withCentre(ledCentre);

            bool isLit = ledPosition <= value;

            // LED glow
            if (isLit)
            {
                g.setColour(XtremeColors::ledBlueGlow);
                g.fillEllipse(ledBounds.expanded(2));
            }

            // LED body
            g.setColour(isLit ? XtremeColors::ledBlueBright : XtremeColors::buttonInset);
            g.fillEllipse(ledBounds);
        }
    }

    void XtremeLeadLookAndFeel::drawLCDDisplay(juce::Graphics& g, juce::Rectangle<float> bounds,
                                              const juce::String& text, bool withPixelGrid)
    {
        // LCD background
        g.setColour(XtremeColors::lcdBackground);
        g.fillRoundedRectangle(bounds, 2.0f);

        // LCD text
        g.setColour(XtremeColors::lcdAmber);
        g.setFont(getEMUFont(bounds.getHeight() * 0.6f, true));
        g.drawFittedText(text, bounds.toNearestInt(), juce::Justification::centred, 1);

        // Pixel grid effect
        if (withPixelGrid)
        {
            g.setColour(XtremeColors::lcdPixelGrid);
            for (float x = bounds.getX(); x < bounds.getRight(); x += 2)
            {
                g.drawVerticalLine((int)x, bounds.getY(), bounds.getBottom());
            }
            for (float y = bounds.getY(); y < bounds.getBottom(); y += 2)
            {
                g.drawHorizontalLine((int)y, bounds.getX(), bounds.getRight());
            }
        }
    }

    juce::Font XtremeLeadLookAndFeel::getEMUFont(float height, bool isLCD)
    {
        if (isLCD)
        {
            // LCD-style font (monospace)
            return juce::Font(juce::Font::getDefaultMonospacedFontName(), height, juce::Font::bold);
        }
        else
        {
            // Clean sans-serif for panel labels
            return juce::Font("Arial", height, juce::Font::plain);
        }
    }

    // LCD Display component implementation
    void XtremeLCDDisplay::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();

        // LCD background
        g.setColour(XtremeColors::lcdBackground);
        g.fillRoundedRectangle(bounds, 3.0f);

        // LCD border
        g.setColour(XtremeColors::metalPanel);
        g.drawRoundedRectangle(bounds, 3.0f, 1.0f);

        // Draw pixel grid
        drawPixelGrid(g);

        // LCD text
        g.setColour(XtremeColors::lcdAmber);
        g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), bounds.getHeight() * 0.7f, juce::Font::bold));

        auto textBounds = bounds.reduced(4);
        auto justification = centered ? juce::Justification::centred : juce::Justification::centredLeft;
        g.drawFittedText(displayText, textBounds.toNearestInt(), justification, 1);
    }

    void XtremeLCDDisplay::drawPixelGrid(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour(XtremeColors::lcdPixelGrid);

        // Horizontal lines
        for (float y = bounds.getY() + 2; y < bounds.getBottom(); y += 2)
        {
            g.drawHorizontalLine((int)y, bounds.getX() + 2, bounds.getRight() - 2);
        }

        // Vertical lines
        for (float x = bounds.getX() + 2; x < bounds.getRight(); x += 2)
        {
            g.drawVerticalLine((int)x, bounds.getY() + 2, bounds.getBottom() - 2);
        }
    }

    // Encoder implementation
    void XtremeEncoder::paint(juce::Graphics& g)
    {
        getLookAndFeel().drawRotarySlider(g, 0, 0, getWidth(), getHeight(),
                                         (float)getValue() / (float)(getMaximum() - getMinimum()),
                                         0, juce::MathConstants<float>::twoPi, *this);
    }
}