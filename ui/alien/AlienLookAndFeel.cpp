#include "AlienLookAndFeel.h"
#include <cmath>

namespace AlienUI
{
    AlienLookAndFeel::AlienLookAndFeel()
    {
        // Set default colors
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour(juce::Slider::textBoxTextColourId, Colors::textPrimary);
        setColour(juce::Slider::textBoxBackgroundColourId, Colors::bgLayer3.withAlpha(0.8f));
        setColour(juce::Label::textColourId, Colors::textPrimary);
        setColour(juce::TextButton::buttonColourId, Colors::bgLayer3);
        setColour(juce::TextButton::textColourOffId, Colors::textPrimary);
        setColour(juce::ComboBox::backgroundColourId, Colors::bgLayer3);
        setColour(juce::ComboBox::textColourId, Colors::textPrimary);
        setColour(juce::PopupMenu::backgroundColourId, Colors::bgLayer2);
        setColour(juce::PopupMenu::textColourId, Colors::textPrimary);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, Colors::cosmicBlue.withAlpha(0.3f));
    }
    
    void AlienLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                            float sliderPosProportional, float rotaryStartAngle,
                                            float rotaryEndAngle, juce::Slider& slider)
    {
        auto bounds = juce::Rectangle<float>(x, y, width, height);
        auto centre = bounds.getCentre();
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.4f;
        
        // Background glow
        drawGlowEffect(g, bounds, Colors::cosmicBlue, 0.3f);
        
        // Outer ring (track)
        g.setColour(Colors::knobTrack);
        g.drawEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 2.0f);
        
        // Energy ring
        float energy = sliderPosProportional;
        drawEnergyRing(g, bounds.reduced(radius * 0.3f), energy, Colors::cosmicBlue);
        
        // Value arc
        auto arcRadius = radius * 0.85f;
        juce::Path arcPath;
        arcPath.addCentredArc(centre.x, centre.y, arcRadius, arcRadius,
                             0.0f, rotaryStartAngle, rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle), true);
        
        g.setColour(Colors::plasmaGlow);
        g.strokePath(arcPath, juce::PathStrokeType(3.0f));
        
        // Center knob
        auto knobRadius = radius * 0.6f;
        g.setColour(Colors::bgLayer3);
        g.fillEllipse(centre.x - knobRadius, centre.y - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f);
        
        // Value indicator line
        auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        auto indicatorStart = centre.getPointOnCircumference(knobRadius * 0.6f, angle);
        auto indicatorEnd = centre.getPointOnCircumference(knobRadius * 0.95f, angle);
        
        g.setColour(Colors::knobValue);
        g.drawLine(indicatorStart.x, indicatorStart.y, indicatorEnd.x, indicatorEnd.y, 2.0f);
        
        // Draw parameter glyph
        auto glyphBounds = bounds.reduced(radius * 1.5f);
        g.setColour(Colors::textSecondary);
        g.setFont(Glyphs::createGlyphFont(glyphBounds.getHeight()));
        g.drawText(Glyphs::getParameterGlyph(slider.getName()), glyphBounds, juce::Justification::centred);
        
        // Mouse interaction glow
        if (slider.isMouseOverOrDragging())
        {
            drawGlowEffect(g, bounds, Colors::plasmaGlow, slider.isMouseButtonDown() ? 0.6f : 0.4f);
        }
    }
    
    void AlienLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                           float sliderPos, float minSliderPos, float maxSliderPos,
                                           const juce::Slider::SliderStyle style, juce::Slider& slider)
    {
        auto bounds = juce::Rectangle<float>(x, y, width, height);
        
        if (style == juce::Slider::LinearVertical)
        {
            // Vertical slider track
            auto trackWidth = 4.0f;
            auto trackX = bounds.getCentreX() - trackWidth * 0.5f;
            
            g.setColour(Colors::knobTrack);
            g.fillRoundedRectangle(trackX, y, trackWidth, height, 2.0f);
            
            // Value fill
            auto fillHeight = sliderPos - y;
            g.setColour(Colors::plasmaGlow);
            g.fillRoundedRectangle(trackX, sliderPos, trackWidth, height - fillHeight, 2.0f);
            
            // Thumb
            auto thumbSize = 16.0f;
            auto thumbBounds = juce::Rectangle<float>(bounds.getCentreX() - thumbSize * 0.5f,
                                                     sliderPos - thumbSize * 0.5f,
                                                     thumbSize, thumbSize);
            
            drawGlowEffect(g, thumbBounds.expanded(4.0f), Colors::cosmicBlue, 0.5f);
            g.setColour(Colors::knobValue);
            g.fillEllipse(thumbBounds);
        }
    }
    
    void AlienLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                               const juce::Colour& backgroundColour,
                                               bool shouldDrawButtonAsHighlighted,
                                               bool shouldDrawButtonAsDown)
    {
        auto bounds = button.getLocalBounds().toFloat();
        
        // Background with state-based color
        auto bgColor = shouldDrawButtonAsDown ? Colors::cosmicBlue.darker(0.2f) :
                      shouldDrawButtonAsHighlighted ? Colors::cosmicBlue.darker(0.4f) :
                      Colors::bgLayer3;
        
        g.setColour(bgColor);
        g.fillRoundedRectangle(bounds, 4.0f);
        
        // Border glow
        if (shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown)
        {
            drawGlowEffect(g, bounds, Colors::plasmaGlow, shouldDrawButtonAsDown ? 0.8f : 0.5f);
        }
        
        // Border
        g.setColour(Colors::cosmicBlue.withAlpha(0.5f));
        g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
    }
    
    void AlienLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button,
                                         bool shouldDrawButtonAsHighlighted,
                                         bool shouldDrawButtonAsDown)
    {
        auto bounds = button.getLocalBounds().toFloat();
        auto textColor = shouldDrawButtonAsDown ? Colors::starWhite :
                        shouldDrawButtonAsHighlighted ? Colors::textPrimary :
                        Colors::textSecondary;
        
        Glyphs::drawAlienText(g, button.getButtonText(), bounds, juce::Justification::centred,
                             shouldDrawButtonAsDown ? Colors::plasmaGlow : Colors::cosmicBlue);
    }
    
    juce::Font AlienLookAndFeel::getLabelFont(juce::Label& label)
    {
        return Glyphs::createAlienFont(label.getFont().getHeight());
    }
    
    void AlienLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& label)
    {
        auto bounds = label.getLocalBounds().toFloat();
        
        if (!label.isBeingEdited())
        {
            auto alpha = label.isEnabled() ? 1.0f : 0.5f;
            auto textColor = label.findColour(juce::Label::textColourId).withAlpha(alpha);
            
            Glyphs::drawAlienText(g, label.getText(), bounds, label.getJustificationType(),
                                 Colors::cosmicBlue.withAlpha(alpha * 0.3f));
        }
        else
        {
            // Editor mode
            g.setColour(Colors::bgLayer3);
            g.fillRoundedRectangle(bounds, 2.0f);
            
            g.setColour(Colors::cosmicBlue.withAlpha(0.5f));
            g.drawRoundedRectangle(bounds, 2.0f, 1.0f);
        }
    }
    
    void AlienLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                                        int buttonX, int buttonY, int buttonW, int buttonH,
                                        juce::ComboBox& box)
    {
        auto bounds = juce::Rectangle<float>(0, 0, width, height);
        
        // Background
        g.setColour(Colors::bgLayer3);
        g.fillRoundedRectangle(bounds, 4.0f);
        
        // Border with glow
        if (box.isMouseOver() || isButtonDown)
        {
            drawGlowEffect(g, bounds, Colors::cosmicBlue, isButtonDown ? 0.6f : 0.3f);
        }
        
        g.setColour(Colors::cosmicBlue.withAlpha(0.5f));
        g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
        
        // Arrow
        juce::Path arrowPath;
        auto arrowBounds = juce::Rectangle<float>(buttonX, buttonY, buttonW, buttonH);
        auto arrowSize = juce::jmin(arrowBounds.getWidth(), arrowBounds.getHeight()) * 0.4f;
        auto arrowCentre = arrowBounds.getCentre();
        
        arrowPath.startNewSubPath(arrowCentre.x - arrowSize * 0.5f, arrowCentre.y - arrowSize * 0.25f);
        arrowPath.lineTo(arrowCentre.x, arrowCentre.y + arrowSize * 0.25f);
        arrowPath.lineTo(arrowCentre.x + arrowSize * 0.5f, arrowCentre.y - arrowSize * 0.25f);
        
        g.setColour(Colors::textPrimary);
        g.strokePath(arrowPath, juce::PathStrokeType(2.0f));
    }
    
    void AlienLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height)
    {
        auto bounds = juce::Rectangle<float>(0, 0, width, height);
        
        // Background with subtle gradient
        g.setGradientFill(juce::ColourGradient(Colors::bgLayer2, bounds.getTopLeft(),
                                               Colors::bgLayer1, bounds.getBottomRight(), false));
        g.fillRoundedRectangle(bounds, 4.0f);
        
        // Border glow
        drawGlowEffect(g, bounds, Colors::cosmicBlue, 0.2f);
        
        g.setColour(Colors::cosmicBlue.withAlpha(0.3f));
        g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
    }
    
    void AlienLookAndFeel::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                                            bool isSeparator, bool isActive, bool isHighlighted,
                                            bool isTicked, bool hasSubMenu,
                                            const juce::String& text,
                                            const juce::String& shortcutKeyText,
                                            const juce::Drawable* icon,
                                            const juce::Colour* textColour)
    {
        if (isSeparator)
        {
            auto bounds = area.toFloat();
            g.setColour(Colors::cosmicBlue.withAlpha(0.2f));
            g.drawLine(bounds.getX() + 10, bounds.getCentreY(),
                      bounds.getRight() - 10, bounds.getCentreY(), 1.0f);
            return;
        }
        
        auto bounds = area.toFloat();
        
        if (isHighlighted && isActive)
        {
            g.setColour(Colors::cosmicBlue.withAlpha(0.2f));
            g.fillRoundedRectangle(bounds.reduced(2.0f), 2.0f);
        }
        
        auto textBounds = bounds.reduced(10.0f, 0.0f);
        auto textColor = isActive ? (isHighlighted ? Colors::textPrimary : Colors::textSecondary)
                                  : Colors::textSecondary.withAlpha(0.5f);
        
        g.setColour(textColor);
        g.setFont(Glyphs::createAlienFont(14.0f));
        g.drawText(text, textBounds, juce::Justification::centredLeft);
        
        if (isTicked)
        {
            auto tickBounds = bounds.removeFromLeft(30.0f);
            g.drawText(Glyphs::energyHigh, tickBounds, juce::Justification::centred);
        }
    }
    
    void AlienLookAndFeel::drawScrollbar(juce::Graphics& g, juce::ScrollBar& scrollbar,
                                         int x, int y, int width, int height,
                                         bool isScrollbarVertical, int thumbStartPosition,
                                         int thumbSize, bool isMouseOver, bool isMouseDown)
    {
        auto bounds = juce::Rectangle<float>(x, y, width, height);
        
        // Track
        g.setColour(Colors::bgLayer3.withAlpha(0.3f));
        g.fillRoundedRectangle(bounds, 2.0f);
        
        // Thumb
        auto thumbBounds = isScrollbarVertical
            ? bounds.withY((float)thumbStartPosition).withHeight((float)thumbSize)
            : bounds.withX((float)thumbStartPosition).withWidth((float)thumbSize);
        
        auto thumbColor = isMouseDown ? Colors::plasmaGlow :
                         isMouseOver ? Colors::cosmicBlue : Colors::knobTrack;
        
        g.setColour(thumbColor);
        g.fillRoundedRectangle(thumbBounds.reduced(2.0f), 2.0f);
    }
    
    void AlienLookAndFeel::drawAlienKnob(juce::Graphics& g, juce::Rectangle<float> bounds,
                                         float value, const juce::String& label,
                                         bool isMouseOver, bool isMouseDown)
    {
        // Central implementation for consistent knob drawing
        auto centre = bounds.getCentre();
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.45f;
        
        // Outer glow based on interaction state
        if (isMouseOver || isMouseDown)
        {
            drawGlowEffect(g, bounds, Colors::plasmaGlow, isMouseDown ? 0.8f : 0.5f);
        }
        
        // Main knob body
        juce::ColourGradient knobGradient(Colors::bgLayer3, centre.translated(0, -radius * 0.5f),
                                         Colors::bgLayer2, centre.translated(0, radius * 0.5f), false);
        g.setGradientFill(knobGradient);
        g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
        
        // Value ring
        drawEnergyRing(g, bounds.reduced(radius * 0.2f), value,
                      value > 0.8f ? Colors::energyCritical : Colors::cosmicBlue);
        
        // Label
        auto labelBounds = bounds.removeFromBottom(20.0f);
        Glyphs::drawAlienText(g, label, labelBounds, juce::Justification::centred, Colors::cosmicBlue);
    }
    
    void AlienLookAndFeel::drawEnergyRing(juce::Graphics& g, juce::Rectangle<float> bounds,
                                          float energy, juce::Colour baseColor)
    {
        auto centre = bounds.getCentre();
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        
        // Energy segments
        const int numSegments = 16;
        const float segmentGap = 0.02f;
        const float segmentSize = (juce::MathConstants<float>::twoPi / numSegments) - segmentGap;
        
        for (int i = 0; i < numSegments; ++i)
        {
            float segmentProgress = (float)i / (float)numSegments;
            if (segmentProgress <= energy)
            {
                float angle = segmentProgress * juce::MathConstants<float>::twoPi - juce::MathConstants<float>::halfPi;
                
                juce::Path segment;
                segment.addPieSegment(centre.x - radius, centre.y - radius,
                                     radius * 2.0f, radius * 2.0f,
                                     angle, angle + segmentSize,
                                     radius * 0.7f);
                
                auto segmentColor = baseColor.interpolatedWith(Colors::energyCritical, segmentProgress);
                g.setColour(segmentColor);
                g.fillPath(segment);
            }
        }
    }
    
    void AlienLookAndFeel::drawGlowEffect(juce::Graphics& g, juce::Rectangle<float> bounds,
                                         juce::Colour glowColor, float intensity)
    {
        const int numGlowLayers = 4;
        
        for (int i = numGlowLayers; i > 0; --i)
        {
            float expansion = (float)i * 3.0f;
            float alpha = intensity * (1.0f - ((float)i / (float)numGlowLayers)) * 0.3f;
            
            g.setColour(glowColor.withAlpha(alpha));
            g.drawRoundedRectangle(bounds.expanded(expansion), 4.0f + expansion, 2.0f);
        }
    }
}
