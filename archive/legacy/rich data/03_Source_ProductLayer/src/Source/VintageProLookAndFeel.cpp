/******************************************************************************
 * File: VintageProLookAndFeel.cpp
 * Description: Implementation of authentic vintage pro audio LookAndFeel
 * 
 * Professional vintage aesthetics inspired by Cool Edit Pro, Sound Forge, 
 * and early Pro Tools. Features realistic transport controls, professional
 * VU metering, and classic rotary knobs with authentic industrial styling.
 * 
 * Copyright (c) 2025 Spectral Audio Systems
 ******************************************************************************/

#include "VintageProLookAndFeel.h"

//==============================================================================
// Constructor - Initialize Vintage Pro Audio Aesthetics
//==============================================================================

VintageProLookAndFeel::VintageProLookAndFeel()
{
    // Set professional vintage color scheme
    setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(VintageColors::backgroundDark));
    setColour(juce::DocumentWindow::backgroundColourId, juce::Colour(VintageColors::backgroundDark));
    
    // Professional text styling
    setColour(juce::Label::textColourId, juce::Colour(VintageColors::textPrimary));
    setColour(juce::TextEditor::textColourId, juce::Colour(VintageColors::textPrimary));
    setColour(juce::TextEditor::backgroundColourId, juce::Colour(VintageColors::panelMedium));
    
    // Professional button styling
    setColour(juce::TextButton::buttonColourId, juce::Colour(VintageColors::panelMedium));
    setColour(juce::TextButton::buttonOnColourId, juce::Colour(VintageColors::accentBlue));
    setColour(juce::TextButton::textColourOffId, juce::Colour(VintageColors::textPrimary));
    setColour(juce::TextButton::textColourOnId, juce::Colour(VintageColors::textPrimary));
    
    // Professional slider styling
    setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(VintageColors::accentBlue));
    setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(VintageColors::panelLight));
    setColour(juce::Slider::thumbColourId, juce::Colour(VintageColors::panelLight));
    setColour(juce::Slider::trackColourId, juce::Colour(VintageColors::panelMedium));
    setColour(juce::Slider::backgroundColourId, juce::Colour(VintageColors::backgroundDark));
    setColour(juce::Slider::textBoxTextColourId, juce::Colour(VintageColors::textPrimary));
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(VintageColors::panelMedium));
}

//==============================================================================
// Professional Transport Controls (Vintage DAW Style)
//==============================================================================

void VintageProLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button, 
                                                const juce::Colour& backgroundColour,
                                                bool shouldDrawButtonAsHighlighted, 
                                                bool shouldDrawButtonAsDown)
{
    auto area = button.getLocalBounds().toFloat();
    
    // Professional vintage button styling
    drawVintageButton(g, area, shouldDrawButtonAsDown, shouldDrawButtonAsHighlighted, 
                     button.getButtonText(), backgroundColour);
}

void VintageProLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button,
                                          bool shouldDrawButtonAsHighlighted, 
                                          bool shouldDrawButtonAsDown)
{
    auto font = getVintageFont(button.getHeight() * 0.4f, true);
    g.setFont(font);
    
    auto textColour = button.findColour(shouldDrawButtonAsDown ? 
                                       juce::TextButton::textColourOnId : 
                                       juce::TextButton::textColourOffId);
    
    // Professional text glow effect for visibility
    if (shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown)
    {
        g.setColour(textColour.withAlpha(0.3f));
        g.drawText(button.getButtonText(), button.getLocalBounds().expanded(1), 
                  juce::Justification::centred);
    }
    
    g.setColour(textColour);
    g.drawText(button.getButtonText(), button.getLocalBounds(), 
              juce::Justification::centred);
}

//==============================================================================
// Professional Rotary Knobs (Vintage Hardware Style)
//==============================================================================

void VintageProLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                            float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                            juce::Slider& slider)
{
    auto area = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y), 
                                      static_cast<float>(width), static_cast<float>(height));
    
    // Calculate knob angle based on slider position
    auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    
    // Draw professional vintage rotary knob
    drawVintageKnob(g, area, angle, slider.isEnabled());
}

//==============================================================================
// Professional VU Metering (Classic Studio Style)
//==============================================================================

void VintageProLookAndFeel::drawLevelMeter(juce::Graphics& g, int width, int height, float level)
{
    auto area = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
    
    // Draw professional vintage VU meter
    drawVintageMeter(g, area, level, height > width);
}

void VintageProLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                           float sliderPos, float minSliderPos, float maxSliderPos,
                                           juce::Slider::SliderStyle style, juce::Slider& slider)
{
    // Professional linear slider with vintage styling
    auto area = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y), 
                                      static_cast<float>(width), static_cast<float>(height));
    
    bool isVertical = (style == juce::Slider::LinearVertical);
    
    // Draw slider track (recessed channel)
    auto trackArea = area.reduced(2.0f);
    drawVintagePanel(g, trackArea, true); // Recessed
    
    // Draw slider thumb
    juce::Rectangle<float> thumbArea;
    if (isVertical)
    {
        float thumbY = juce::jmap(sliderPos, 0.0f, 1.0f, 
                                 trackArea.getBottom() - 8.0f, trackArea.getY());
        thumbArea = juce::Rectangle<float>(trackArea.getX(), thumbY, trackArea.getWidth(), 16.0f);
    }
    else
    {
        float thumbX = juce::jmap(sliderPos, 0.0f, 1.0f, 
                                 trackArea.getX(), trackArea.getRight() - 16.0f);
        thumbArea = juce::Rectangle<float>(thumbX, trackArea.getY(), 16.0f, trackArea.getHeight());
    }
    
    // Professional slider thumb styling
    g.setColour(juce::Colour(VintageColors::panelLight));
    g.fillRoundedRectangle(thumbArea, 2.0f);
    
    // Thumb highlight
    g.setColour(juce::Colour(VintageColors::textSecondary));
    g.drawRoundedRectangle(thumbArea, 2.0f, 1.0f);
}

//==============================================================================
// Professional Labels and Typography
//==============================================================================

void VintageProLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& label)
{
    auto textArea = label.getBorderSize().subtractedFrom(label.getLocalBounds());
    
    g.setColour(label.findColour(juce::Label::backgroundColourId));
    g.fillRect(textArea);
    
    g.setColour(label.findColour(juce::Label::textColourId));
    g.setFont(getLabelFont(label));
    
    g.drawText(label.getText(), textArea, label.getJustificationType(), 
              true); // Use ellipsis if needed
}

juce::Font VintageProLookAndFeel::getLabelFont(juce::Label& label)
{
    return getVintageFont(static_cast<float>(label.getHeight()) * 0.7f);
}

//==============================================================================
// Professional Panel Styling
//==============================================================================

void VintageProLookAndFeel::fillResizableWindowBackground(juce::Graphics& g, int w, int h,
                                                         const juce::BorderSize<int>& border,
                                                         juce::ResizableWindow& window)
{
    // Professional vintage window background
    g.fillAll(juce::Colour(VintageColors::backgroundDark));
    
    // Subtle texture overlay for authentic vintage feel
    auto area = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h));
    
    // Very subtle noise/grain texture
    juce::Random random(42); // Deterministic seed for consistent texture
    g.setColour(juce::Colour(VintageColors::textSecondary).withAlpha(0.02f));
    
    for (int i = 0; i < 100; ++i)
    {
        float x = random.nextFloat() * w;
        float y = random.nextFloat() * h;
        g.fillRect(x, y, 1.0f, 1.0f);
    }
}

//==============================================================================
// Private: Vintage Drawing Utilities
//==============================================================================

void VintageProLookAndFeel::drawVintageButton(juce::Graphics& g, juce::Rectangle<float> area,
                                             bool isPressed, bool isHighlighted, 
                                             const juce::String& text, juce::Colour baseColour)
{
    // Professional button with subtle 3D effect
    auto buttonArea = area.reduced(1.0f);
    
    juce::Colour buttonColour = baseColour;
    if (isPressed)
        buttonColour = buttonColour.darker(0.3f);
    else if (isHighlighted)
        buttonColour = buttonColour.brighter(0.2f);
    
    // Main button face
    g.setColour(buttonColour);
    g.fillRoundedRectangle(buttonArea, 2.0f);
    
    // Professional beveled edge effect
    if (!isPressed)
    {
        // Top/left highlight
        g.setColour(buttonColour.brighter(0.4f));
        g.drawLine(buttonArea.getX(), buttonArea.getY(), 
                  buttonArea.getRight(), buttonArea.getY(), 1.0f);
        g.drawLine(buttonArea.getX(), buttonArea.getY(), 
                  buttonArea.getX(), buttonArea.getBottom(), 1.0f);
        
        // Bottom/right shadow
        g.setColour(buttonColour.darker(0.4f));
        g.drawLine(buttonArea.getX(), buttonArea.getBottom(), 
                  buttonArea.getRight(), buttonArea.getBottom(), 1.0f);
        g.drawLine(buttonArea.getRight(), buttonArea.getY(), 
                  buttonArea.getRight(), buttonArea.getBottom(), 1.0f);
    }
    else
    {
        // Pressed button border
        g.setColour(buttonColour.darker(0.6f));
        g.drawRoundedRectangle(buttonArea, 2.0f, 1.0f);
    }
}

void VintageProLookAndFeel::drawVintageKnob(juce::Graphics& g, juce::Rectangle<float> area,
                                           float angle, bool isEnabled)
{
    auto knobArea = area.reduced(4.0f);
    auto centre = knobArea.getCentre();
    auto radius = juce::jmin(knobArea.getWidth(), knobArea.getHeight()) * 0.4f;
    
    // Knob body (metallic appearance)
    juce::Colour knobColour = isEnabled ? 
        juce::Colour(VintageColors::panelLight) : 
        juce::Colour(VintageColors::panelMedium);
    
    g.setColour(knobColour);
    g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
    
    // Knob highlight (top-left)
    g.setColour(knobColour.brighter(0.6f));
    auto highlightArea = juce::Rectangle<float>(centre.x - radius, centre.y - radius, 
                                               radius * 1.4f, radius * 1.4f);
    g.fillEllipse(highlightArea);
    
    // Knob shadow (bottom-right)
    g.setColour(knobColour.darker(0.4f));
    auto shadowArea = juce::Rectangle<float>(centre.x - radius * 0.6f, centre.y - radius * 0.6f, 
                                            radius * 1.4f, radius * 1.4f);
    g.fillEllipse(shadowArea);
    
    // Knob center
    g.setColour(knobColour);
    g.fillEllipse(centre.x - radius * 0.8f, centre.y - radius * 0.8f, 
                 radius * 1.6f, radius * 1.6f);
    
    // Position indicator (pointer line)
    if (isEnabled)
    {
        g.setColour(juce::Colour(VintageColors::accentBlue));
        auto pointerLength = radius * 0.6f;
        auto pointerX = centre.x + std::cos(angle - juce::MathConstants<float>::halfPi) * pointerLength;
        auto pointerY = centre.y + std::sin(angle - juce::MathConstants<float>::halfPi) * pointerLength;
        
        g.drawLine(centre.x, centre.y, pointerX, pointerY, 2.0f);
        
        // Pointer tip
        g.fillEllipse(pointerX - 2.0f, pointerY - 2.0f, 4.0f, 4.0f);
    }
}

void VintageProLookAndFeel::drawVintageMeter(juce::Graphics& g, juce::Rectangle<float> area,
                                            float level, bool isVertical)
{
    // Professional VU meter with segmented LED display
    auto meterArea = area.reduced(2.0f);
    
    // Meter background (recessed)
    g.setColour(juce::Colour(VintageColors::borderDark));
    g.fillRoundedRectangle(meterArea, 1.0f);
    
    // LED segments
    int numSegments = isVertical ? 20 : 10;
    float segmentSize = (isVertical ? meterArea.getHeight() : meterArea.getWidth()) / numSegments;
    
    for (int i = 0; i < numSegments; ++i)
    {
        float segmentLevel = static_cast<float>(i) / numSegments;
        bool isLit = level >= segmentLevel;
        
        // Segment color based on level
        juce::Colour segmentColour;
        if (segmentLevel < 0.7f)
            segmentColour = juce::Colour(VintageColors::meterGreen);
        else if (segmentLevel < 0.9f)
            segmentColour = juce::Colour(VintageColors::meterAmber);
        else
            segmentColour = juce::Colour(VintageColors::meterRed);
        
        if (!isLit)
            segmentColour = juce::Colour(VintageColors::ledOff);
        
        // Draw LED segment
        juce::Rectangle<float> segmentRect;
        if (isVertical)
        {
            float y = meterArea.getBottom() - (i + 1) * segmentSize;
            segmentRect = juce::Rectangle<float>(meterArea.getX() + 1.0f, y + 1.0f, 
                                                meterArea.getWidth() - 2.0f, segmentSize - 2.0f);
        }
        else
        {
            float x = meterArea.getX() + i * segmentSize;
            segmentRect = juce::Rectangle<float>(x + 1.0f, meterArea.getY() + 1.0f, 
                                                segmentSize - 2.0f, meterArea.getHeight() - 2.0f);
        }
        
        g.setColour(segmentColour);
        g.fillRoundedRectangle(segmentRect, 1.0f);
        
        // LED glow effect when lit
        if (isLit)
        {
            g.setColour(segmentColour.withAlpha(0.3f));
            g.fillRoundedRectangle(segmentRect.expanded(1.0f), 2.0f);
        }
    }
}

void VintageProLookAndFeel::drawVintagePanel(juce::Graphics& g, juce::Rectangle<float> area,
                                            bool isRecessed)
{
    juce::Colour panelColour = juce::Colour(VintageColors::panelMedium);
    
    g.setColour(panelColour);
    g.fillRoundedRectangle(area, 2.0f);
    
    // Panel border effect
    if (isRecessed)
    {
        // Recessed panel (inward bevel)
        g.setColour(panelColour.darker(0.4f));
        g.drawLine(area.getX(), area.getY(), area.getRight(), area.getY(), 1.0f);
        g.drawLine(area.getX(), area.getY(), area.getX(), area.getBottom(), 1.0f);
        
        g.setColour(panelColour.brighter(0.4f));
        g.drawLine(area.getX(), area.getBottom(), area.getRight(), area.getBottom(), 1.0f);
        g.drawLine(area.getRight(), area.getY(), area.getRight(), area.getBottom(), 1.0f);
    }
    else
    {
        // Raised panel (outward bevel)
        g.setColour(panelColour.brighter(0.4f));
        g.drawLine(area.getX(), area.getY(), area.getRight(), area.getY(), 1.0f);
        g.drawLine(area.getX(), area.getY(), area.getX(), area.getBottom(), 1.0f);
        
        g.setColour(panelColour.darker(0.4f));
        g.drawLine(area.getX(), area.getBottom(), area.getRight(), area.getBottom(), 1.0f);
        g.drawLine(area.getRight(), area.getY(), area.getRight(), area.getBottom(), 1.0f);
    }
}

void VintageProLookAndFeel::drawVintageBorder(juce::Graphics& g, juce::Rectangle<float> area,
                                             int thickness, bool isInset)
{
    juce::Colour borderColour = juce::Colour(VintageColors::panelLight);
    
    if (isInset)
    {
        g.setColour(borderColour.darker(0.6f));
        g.drawRect(area, static_cast<float>(thickness));
    }
    else
    {
        g.setColour(borderColour.brighter(0.4f));
        g.drawRect(area, static_cast<float>(thickness));
    }
}

juce::Font VintageProLookAndFeel::getVintageFont(float height, bool isBold) const
{
    // Professional vintage typography (clean, technical font)
    return juce::Font(height, isBold ? juce::Font::FontStyleFlags::bold : juce::Font::FontStyleFlags::plain);
}