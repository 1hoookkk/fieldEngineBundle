#include <cmath>
#include "WavesPS22LookAndFeel.h"

using juce::Colours;

namespace
{
    constexpr float kKnobOutline = 1.5f;
    constexpr float kKnobInnerInset = 3.0f;
}

WavesPS22LookAndFeel::WavesPS22LookAndFeel()
{
    createFonts();

    setColour(juce::ResizableWindow::backgroundColourId, Colors::metalDark());
    setColour(juce::Slider::thumbColourId, Colors::metalLight());
    setColour(juce::Slider::trackColourId, Colors::blackPanel().brighter(0.08f));
    setColour(juce::Label::textColourId, Colors::textDark());
    setColour(juce::TextButton::buttonColourId, Colors::metalMid());
    setColour(juce::TextButton::buttonOnColourId, Colors::metalDark());
}

void WavesPS22LookAndFeel::createFonts()
{
    smallCapsFont = juce::Font(juce::Font::getDefaultSansSerifFontName(), 11.0f, juce::Font::bold);
    smallCapsFont.setExtraKerningFactor(0.08f);

    digitalFont = juce::Font(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::bold);
    digitalFont.setExtraKerningFactor(0.04f);
}

//==============================================================================
void WavesPS22LookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                            float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                            juce::Slider& slider)
{
    juce::ignoreUnused(slider);

    auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                         static_cast<float>(width), static_cast<float>(height)).reduced(kKnobInnerInset);

    auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    auto centre = bounds.getCentre();

    // Knob body
    drawBrushedMetal(g, bounds, true);

    g.setColour(Colors::shadowDark().withAlpha(0.6f));
    g.drawEllipse(bounds, kKnobOutline);

    auto indicatorRadius = radius * 0.7f;
    auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    auto indicatorEnd = juce::Point<float>(centre.x + indicatorRadius * std::cos(angle - juce::MathConstants<float>::halfPi),
                                           centre.y + indicatorRadius * std::sin(angle - juce::MathConstants<float>::halfPi));

    g.setColour(Colors::textWhite());
    g.drawLine({ centre, indicatorEnd }, 2.2f);

    g.setColour(Colors::shadowDark().withAlpha(0.3f));
    g.fillEllipse(bounds.withSizeKeepingCentre(radius * 0.35f, radius * 0.35f));

    // Tick marks
    g.setColour(Colors::shadowDark().withAlpha(0.4f));
    const int tickCount = 21;
    auto tickStart = radius * 0.82f;
    auto tickEnd = radius * 0.92f;
    for (int i = 0; i < tickCount; ++i)
    {
        auto fraction = static_cast<float>(i) / static_cast<float>(tickCount - 1);
        auto tickAngle = rotaryStartAngle + fraction * (rotaryEndAngle - rotaryStartAngle);
        auto sinAngle = std::sin(tickAngle - juce::MathConstants<float>::halfPi);
        auto cosAngle = std::cos(tickAngle - juce::MathConstants<float>::halfPi);
        juce::Point<float> tickFrom(centre.x + tickStart * cosAngle,
                                    centre.y + tickStart * sinAngle);
        juce::Point<float> tickTo(centre.x + tickEnd * cosAngle,
                                  centre.y + tickEnd * sinAngle);
        g.drawLine({ tickFrom, tickTo }, (i % 5 == 0) ? 1.2f : 0.6f);
    }
}

void WavesPS22LookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                            float sliderPos, float minSliderPos, float maxSliderPos,
                                            const juce::Slider::SliderStyle style, juce::Slider& slider)
{
    juce::ignoreUnused(style, slider);

    auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                         static_cast<float>(width), static_cast<float>(height));

    auto trackHeight = juce::jmin(8.0f, bounds.getHeight());
    auto track = bounds.withSizeKeepingCentre(bounds.getWidth(), trackHeight).reduced(0.0f, 1.0f);

    drawInsetPanel(g, track);

    if (maxSliderPos > minSliderPos)
    {
        auto fillBounds = track;
        fillBounds.setWidth(sliderPos - minSliderPos);
        fillBounds = fillBounds.withTrimmedBottom(trackHeight * 0.05f).withTrimmedTop(trackHeight * 0.05f);
        fillBounds = fillBounds.withWidth(juce::jlimit(0.0f, track.getWidth(), fillBounds.getWidth()));

        juce::ColourGradient meterGradient(Colors::ledGreen().brighter(0.3f), fillBounds.getX(), fillBounds.getCentreY(),
                                           Colors::ledGreen().darker(0.4f), fillBounds.getRight(), fillBounds.getCentreY(), false);
        g.setGradientFill(meterGradient);
        g.fillRoundedRectangle(fillBounds, trackHeight * 0.45f);
    }

    auto thumbSize = juce::jmin(16.0f, bounds.getHeight());
    juce::Rectangle<float> thumb(sliderPos - thumbSize * 0.5f,
                                 track.getCentreY() - thumbSize * 0.75f,
                                 thumbSize, thumbSize * 1.5f);

    drawBrushedMetal(g, thumb, true);
    g.setColour(Colors::shadowDark().withAlpha(0.8f));
    g.drawRoundedRectangle(thumb, 3.0f, 1.2f);
}

void WavesPS22LookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                                            bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat();
    drawInsetPanel(g, bounds);

    if (shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown)
    {
        g.setColour(Colors::shadowLight().withAlpha(shouldDrawButtonAsDown ? 0.4f : 0.25f));
        g.fillRoundedRectangle(bounds.reduced(1.0f), 4.0f);
    }

    auto ledCentre = bounds.getCentre().translated(-(bounds.getWidth() * 0.3f), 0.0f);
    drawLED(g, ledCentre, 4.0f, Colors::ledAmber(), button.getToggleState());

    g.setColour(Colors::textDark());
    g.setFont(smallCapsFont);
    g.drawText(button.getButtonText().toUpperCase(), button.getLocalBounds(), juce::Justification::centredRight, false);
}

void WavesPS22LookAndFeel::drawLabel(juce::Graphics& g, juce::Label& label)
{
    auto bounds = label.getLocalBounds().toFloat();
    g.setColour(Colours::transparentBlack);
    g.fillRect(bounds);

    g.setColour(Colors::textWhite());
    g.setFont(smallCapsFont);
    g.drawFittedText(label.getText().toUpperCase(), label.getLocalBounds(), label.getJustificationType(), 1);
}

void WavesPS22LookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                                        int buttonX, int buttonY, int buttonW, int buttonH,
                                        juce::ComboBox& box)
{
    juce::ignoreUnused(buttonX, buttonY, buttonW, buttonH);
    auto bounds = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
    drawInsetPanel(g, bounds);

    if (isButtonDown)
    {
        g.setColour(Colors::shadowLight().withAlpha(0.25f));
        g.fillRoundedRectangle(bounds.reduced(1.0f), 3.0f);
    }

    g.setColour(Colors::textDark());
    g.setFont(smallCapsFont);
    g.drawText(box.getText(), box.getLocalBounds(), juce::Justification::centred);
}

juce::Font WavesPS22LookAndFeel::getLabelFont(juce::Label& label)
{
    juce::ignoreUnused(label);
    return smallCapsFont;
}

juce::Font WavesPS22LookAndFeel::getComboBoxFont(juce::ComboBox& box)
{
    juce::ignoreUnused(box);
    return smallCapsFont;
}

juce::Font WavesPS22LookAndFeel::getTextButtonFont(juce::TextButton& button, int buttonHeight)
{
    juce::ignoreUnused(button);
    auto font = smallCapsFont;
    font.setHeight(juce::jmin(13.0f, static_cast<float>(buttonHeight) * 0.6f));
    return font;
}

//==============================================================================
void WavesPS22LookAndFeel::drawBrushedMetal(juce::Graphics& g, juce::Rectangle<float> bounds, bool isVertical)
{
    juce::ColourGradient gradient(Colors::metalLight(), bounds.getX(), bounds.getY(),
                                  Colors::metalMid(), bounds.getRight(), bounds.getBottom(), false);

    if (isVertical)
        gradient.point2 = { bounds.getX(), bounds.getBottom() };

    g.setGradientFill(gradient);
    g.fillRoundedRectangle(bounds, juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.15f);

    g.saveState();
    g.reduceClipRegion(bounds.reduced(1.0f));
    g.setColour(juce::Colour(0x08000000));

    if (isVertical)
    {
        for (float x = bounds.getX(); x < bounds.getRight(); x += 2.0f)
            g.drawVerticalLine(static_cast<int>(std::round(x)), bounds.getY(), bounds.getBottom());
    }
    else
    {
        for (float y = bounds.getY(); y < bounds.getBottom(); y += 2.0f)
            g.drawHorizontalLine(static_cast<int>(std::round(y)), bounds.getX(), bounds.getRight());
    }

    g.restoreState();

    juce::ColourGradient highlight(juce::Colour(0x10FFFFFF), bounds.getX(), bounds.getY(),
                                   juce::Colour(0x00000000), bounds.getX(), bounds.getBottom(), false);
    if (! isVertical)
        highlight = juce::ColourGradient(juce::Colour(0x10FFFFFF), bounds.getX(), bounds.getY(),
                                         juce::Colour(0x00000000), bounds.getRight(), bounds.getY(), false);

    g.setGradientFill(highlight);
    g.fillRoundedRectangle(bounds, juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.15f);
}

void WavesPS22LookAndFeel::drawInsetPanel(juce::Graphics& g, juce::Rectangle<float> bounds, float depth)
{
    juce::ColourGradient outer(Colors::metalDark(), bounds.getX(), bounds.getY(),
                               Colors::metalMid(), bounds.getRight(), bounds.getBottom(), false);
    g.setGradientFill(outer);
    g.fillRoundedRectangle(bounds, juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.2f);

    auto inner = bounds.reduced(depth);
    g.setColour(Colors::blackPanel());
    g.fillRoundedRectangle(inner, juce::jmax(2.0f, juce::jmin(inner.getWidth(), inner.getHeight()) * 0.1f));
}

void WavesPS22LookAndFeel::drawLED(juce::Graphics& g, juce::Point<float> centre, float radius,
                                   juce::Colour colour, bool isOn)
{
    auto ledColour = isOn ? colour : colour.withMultipliedBrightness(0.25f);

    auto glow = juce::ColourGradient(ledColour.withAlpha(isOn ? 0.35f : 0.12f), centre.x, centre.y,
                                     juce::Colours::transparentBlack, centre.x, centre.y + radius * 2.8f, true);
    g.setGradientFill(glow);
    g.fillEllipse({ centre.x - radius * 2.0f,
                    centre.y - radius * 2.0f,
                    radius * 4.0f,
                    radius * 4.0f });

    g.setColour(ledColour);
    g.fillEllipse({ centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f });

    g.setColour(Colours::white.withAlpha(0.35f));
    g.drawEllipse({ centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f }, 0.8f);
}

void WavesPS22LookAndFeel::drawBevel(juce::Graphics& g, juce::Rectangle<float> bounds,
                                     bool isRaised, float thickness)
{
    auto light = Colors::shadowLight().withAlpha(isRaised ? 0.5f : 0.1f);
    auto dark = Colors::shadowDark().withAlpha(isRaised ? 0.7f : 0.3f);

    g.setColour(isRaised ? light : dark);
    g.fillRect({ bounds.getX(), bounds.getY(), bounds.getWidth(), thickness });
    g.fillRect({ bounds.getX(), bounds.getY(), thickness, bounds.getHeight() });

    g.setColour(isRaised ? dark : light);
    g.fillRect({ bounds.getX(), bounds.getBottom() - thickness, bounds.getWidth(), thickness });
    g.fillRect({ bounds.getRight() - thickness, bounds.getY(), thickness, bounds.getHeight() });
}

//==============================================================================
HardwareMorphPad::HardwareMorphPad()
{
    setRepaintsOnMouseActivity(true);
}

void HardwareMorphPad::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    WavesPS22LookAndFeel::drawInsetPanel(g, bounds, 3.0f);

    auto inner = bounds.reduced(5.0f);
    g.setColour(WavesPS22LookAndFeel::Colors::blackPanel().withBrightness(0.12f));
    g.fillRoundedRectangle(inner, 6.0f);

    g.setColour(WavesPS22LookAndFeel::Colors::shadowDark().withAlpha(0.5f));
    g.drawRoundedRectangle(inner, 6.0f, 1.0f);

    // Crosshair
    auto centre = inner.getCentre();
    g.setColour(WavesPS22LookAndFeel::Colors::shadowLight().withAlpha(0.15f));
    g.drawLine(centre.x, inner.getY() + 4.0f, centre.x, inner.getBottom() - 4.0f, 1.0f);
    g.drawLine(inner.getX() + 4.0f, centre.y, inner.getRight() - 4.0f, centre.y, 1.0f);

    auto ledCentre = juce::Point<float>(inner.getX() + inner.getWidth() * xPos.load(),
                                        inner.getY() + inner.getHeight() * (1.0f - yPos.load()));

    lastIndicatorBounds = juce::Rectangle<float>(ledCentre.x - 10.0f, ledCentre.y - 10.0f, 20.0f, 20.0f);
    WavesPS22LookAndFeel::drawLED(g, ledCentre, 4.2f, WavesPS22LookAndFeel::Colors::ledRed(), true);
}

void HardwareMorphPad::setPosition(float x, float y, juce::NotificationType notification)
{
    auto clampedX = juce::jlimit(0.0f, 1.0f, x);
    auto clampedY = juce::jlimit(0.0f, 1.0f, y);

    if (clampedX == xPos.load() && clampedY == yPos.load())
        return;

    xPos.store(clampedX);
    yPos.store(clampedY);

    repaintIndicator();

    if (notification == juce::sendNotification && onValueChange)
        onValueChange(clampedX, clampedY);
}

void HardwareMorphPad::mouseDown(const juce::MouseEvent& e)
{
    isDragging = true;
    auto local = e.position;
    auto bounds = getLocalBounds().toFloat();
    auto inner = bounds.reduced(5.0f);

    auto relativeX = (local.x - inner.getX()) / juce::jmax(1.0f, inner.getWidth());
    auto relativeY = (local.y - inner.getY()) / juce::jmax(1.0f, inner.getHeight());

    setPosition(relativeX, 1.0f - relativeY, juce::sendNotification);
}

void HardwareMorphPad::mouseDrag(const juce::MouseEvent& e)
{
    if (! isDragging)
        return;

    auto local = e.position;
    auto bounds = getLocalBounds().toFloat();
    auto inner = bounds.reduced(5.0f);

    auto relativeX = (local.x - inner.getX()) / juce::jmax(1.0f, inner.getWidth());
    auto relativeY = (local.y - inner.getY()) / juce::jmax(1.0f, inner.getHeight());

    setPosition(relativeX, 1.0f - relativeY, juce::sendNotification);
}

void HardwareMorphPad::mouseUp(const juce::MouseEvent&)
{
    isDragging = false;
}

void HardwareMorphPad::repaintIndicator()
{
    if (! lastIndicatorBounds.isEmpty())
        repaint(lastIndicatorBounds.getSmallestIntegerContainer());

    auto bounds = getLocalBounds().toFloat().reduced(5.0f);
    auto ledCentre = juce::Point<float>(bounds.getX() + bounds.getWidth() * xPos.load(),
                                        bounds.getY() + bounds.getHeight() * (1.0f - yPos.load()));
    lastIndicatorBounds = juce::Rectangle<float>(ledCentre.x - 12.0f, ledCentre.y - 12.0f, 24.0f, 24.0f);
    repaint(lastIndicatorBounds.getSmallestIntegerContainer());
}

//==============================================================================
LEDButton::LEDButton(const juce::String& text) : juce::TextButton(text)
{
    setClickingTogglesState(false);
    setTriggeredOnMouseDown(false);
}

void LEDButton::paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted,
                            bool shouldDrawButtonAsDown)
{
    auto bounds = getLocalBounds().toFloat();
    WavesPS22LookAndFeel::drawInsetPanel(g, bounds, 1.5f);

    if (shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown)
    {
        g.setColour(WavesPS22LookAndFeel::Colors::shadowLight().withAlpha(shouldDrawButtonAsDown ? 0.45f : 0.25f));
        g.fillRoundedRectangle(bounds.reduced(1.0f), 4.0f);
    }

    auto ledCentre = juce::Point<float>(bounds.getX() + 10.0f, bounds.getCentreY());
    WavesPS22LookAndFeel::drawLED(g, ledCentre, 3.5f, ledColor, ledOn);

    auto font = juce::Font(juce::Font::getDefaultSansSerifFontName(), 10.0f, juce::Font::bold);
    font.setExtraKerningFactor(0.08f);

    g.setColour(WavesPS22LookAndFeel::Colors::textWhite());
    g.setFont(font);
    g.drawText(getButtonText().toUpperCase(), bounds.withTrimmedLeft(18.0f), juce::Justification::centredLeft, false);
}

//==============================================================================
DigitalDisplay::DigitalDisplay()
{
    setInterceptsMouseClicks(false, false);
}

void DigitalDisplay::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour(WavesPS22LookAndFeel::Colors::blackPanel().darker(0.5f));
    g.fillRoundedRectangle(bounds, 3.0f);

    g.setColour(displayColor.withAlpha(0.65f));
    g.drawRoundedRectangle(bounds, 3.0f, 1.0f);

    g.setColour(displayColor);
    auto font = juce::Font(juce::Font::getDefaultMonospacedFontName(), bounds.getHeight() * 0.75f, juce::Font::bold);
    g.setFont(font);
    g.drawText(displayText, getLocalBounds().reduced(4), juce::Justification::centredRight, false);
}

void DigitalDisplay::setValue(float value, const juce::String& unit)
{
    auto text = juce::String(value, 2);
    if (unit.isNotEmpty())
        text << unit;
    setText(text);
}

void DigitalDisplay::setText(const juce::String& text)
{
    if (displayText == text)
        return;

    displayText = text;
    repaint();
}
