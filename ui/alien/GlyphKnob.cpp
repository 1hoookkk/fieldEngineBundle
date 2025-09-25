#include "GlyphKnob.h"
#include <cmath>

namespace AlienUI
{
    GlyphKnob::GlyphKnob(const juce::String& name)
    {
        setName(name);
        setSliderStyle(juce::Slider::RotaryVerticalDrag);
        setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        setRotaryParameters(juce::MathConstants<float>::pi * 1.2f,
                           juce::MathConstants<float>::pi * 2.8f,
                           true);
        
        // Set up animation timer
        animationTimer.onTimerCallback = [this]() { timerCallback(); };
        animationTimer.startTimerHz(30);
        
        // Auto-detect glyph from name
        setGlyphFromParameter();
    }
    
    void GlyphKnob::paint(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();
        auto knobBounds = bounds.reduced(5.0f);
        
        // Center point and radius
        auto centre = knobBounds.getCentre();
        auto radius = juce::jmin(knobBounds.getWidth(), knobBounds.getHeight()) * 0.4f;
        
        // Background glow based on energy and interaction
        if (interactionGlowEnabled && (isInteracting || glowIntensity > 0.01f))
        {
            g.setColour(Colors::plasmaGlow.withAlpha(glowIntensity * 0.4f));
            g.fillEllipse(centre.x - radius * 1.5f, centre.y - radius * 1.5f,
                         radius * 3.0f, radius * 3.0f);
        }
        
        // Outer ring with energy indication
        auto energyColor = energyLevel < 0.33f ? Colors::energyLow :
                          energyLevel < 0.66f ? Colors::energyMid :
                          energyLevel < 0.9f ? Colors::energyHigh : Colors::energyCritical;
        
        g.setColour(Colors::knobTrack.interpolatedWith(energyColor, energyLevel * 0.5f));
        g.drawEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 2.0f);
        
        // Value arc
        auto startAngle = getRotaryParameters().startAngleRadians;
        auto endAngle = getRotaryParameters().endAngleRadians;
        auto valueAngle = startAngle + (getValue() - getMinimum()) / (getMaximum() - getMinimum()) * (endAngle - startAngle);
        
        juce::Path valuePath;
        valuePath.addCentredArc(centre.x, centre.y, radius * 0.9f, radius * 0.9f,
                               0.0f, startAngle, valueAngle, true);
        
        // Gradient stroke for value arc
        juce::ColourGradient arcGradient(Colors::cosmicBlue, centre.getPointOnCircumference(radius, startAngle),
                                        Colors::plasmaGlow, centre.getPointOnCircumference(radius, valueAngle), false);
        g.setGradientFill(arcGradient);
        g.strokePath(valuePath, juce::PathStrokeType(3.0f));
        
        // Pulse effect on arc
        if (pulseEnabled && pulsePhase > 0.0f)
        {
            g.setColour(Colors::starWhite.withAlpha(std::sin(pulsePhase) * 0.3f));
            g.strokePath(valuePath, juce::PathStrokeType(5.0f));
        }
        
        // Center knob with gradient
        auto knobRadius = radius * 0.65f;
        juce::ColourGradient knobGradient(Colors::bgLayer3.brighter(0.1f), centre.translated(0, -knobRadius * 0.3f),
                                         Colors::bgLayer2, centre.translated(0, knobRadius * 0.3f), false);
        g.setGradientFill(knobGradient);
        g.fillEllipse(centre.x - knobRadius, centre.y - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f);
        
        // Inner ring
        g.setColour(Colors::cosmicBlue.withAlpha(0.3f));
        g.drawEllipse(centre.x - knobRadius, centre.y - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f, 1.0f);
        
        // Value indicator
        auto indicatorLength = knobRadius * 0.8f;
        auto indicatorEnd = centre.getPointOnCircumference(indicatorLength, valueAngle);
        
        g.setColour(Colors::knobValue);
        g.drawLine(centre.x, centre.y, indicatorEnd.x, indicatorEnd.y, 2.0f);
        
        // Glyph in center
        if (customGlyph.isNotEmpty())
        {
            auto glyphBounds = juce::Rectangle<float>(centre.x - knobRadius * 0.6f,
                                                     centre.y - knobRadius * 0.4f,
                                                     knobRadius * 1.2f,
                                                     knobRadius * 0.8f);
            
            g.setColour(Colors::textSecondary.interpolatedWith(Colors::plasmaGlow, energyLevel * 0.3f));
            g.setFont(Glyphs::createGlyphFont(glyphBounds.getHeight()));
            g.drawText(customGlyph, glyphBounds, juce::Justification::centred);
        }
        
        // Value text below knob
        auto valueBounds = bounds.removeFromBottom(20.0f);
        juce::String valueText = formatValue(getValue());
        
        Glyphs::drawAlienText(g, valueText, valueBounds, juce::Justification::centred,
                             isMouseOver() ? Colors::plasmaGlow : Colors::cosmicBlue);
        
        // Parameter name above knob
        if (getName().isNotEmpty())
        {
            auto nameBounds = bounds.removeFromTop(15.0f);
            g.setColour(Colors::textSecondary);
            g.setFont(Glyphs::createAlienFont(10.0f));
            g.drawText(getName().toUpperCase(), nameBounds, juce::Justification::centred);
        }
    }
    
    void GlyphKnob::mouseEnter(const juce::MouseEvent&)
    {
        isInteracting = true;
        updateGlowIntensity();
    }
    
    void GlyphKnob::mouseExit(const juce::MouseEvent&)
    {
        isInteracting = false;
        updateGlowIntensity();
    }
    
    void GlyphKnob::mouseDown(const juce::MouseEvent& event)
    {
        energyLevel = 1.0f;
        juce::Slider::mouseDown(event);
    }
    
    void GlyphKnob::mouseUp(const juce::MouseEvent& event)
    {
        juce::Slider::mouseUp(event);
    }
    
    void GlyphKnob::mouseDrag(const juce::MouseEvent& event)
    {
        // Update energy based on drag speed
        static juce::Point<float> lastPos;
        float dragDistance = event.position.getDistanceFrom(lastPos);
        energyLevel = juce::jlimit(0.3f, 1.0f, energyLevel + dragDistance * 0.01f);
        lastPos = event.position;
        
        juce::Slider::mouseDrag(event);
    }
    
    void GlyphKnob::timerCallback()
    {
        // Update animations
        const float decayRate = 0.95f;
        const float pulseSpeed = 0.1f;
        
        // Energy decay
        energyLevel *= decayRate;
        energyLevel = juce::jmax(0.0f, energyLevel);
        
        // Glow animation
        updateGlowIntensity();
        
        // Pulse animation
        if (pulseEnabled)
        {
            pulsePhase += pulseSpeed;
            if (pulsePhase > juce::MathConstants<float>::twoPi)
                pulsePhase -= juce::MathConstants<float>::twoPi;
        }
        
        repaint();
    }
    
    void GlyphKnob::updateGlowIntensity()
    {
        float targetGlow = isInteracting ? 0.8f : 0.0f;
        glowIntensity += (targetGlow - glowIntensity) * 0.1f;
    }
    
    juce::String GlyphKnob::formatValue(double value) const
    {
        juce::String result = valuePrefix;
        
        if (decimalPlaces == 0)
            result += juce::String((int)value);
        else
            result += juce::String(value, decimalPlaces);
            
        result += valueSuffix;
        return result;
    }
    
    void GlyphKnob::startPulse()
    {
        pulseEnabled = true;
        pulsePhase = 0.0f;
    }
    
    void GlyphKnob::stopPulse()
    {
        pulseEnabled = false;
        pulsePhase = 0.0f;
    }
    
    // FrequencyKnob implementation
    FrequencyKnob::FrequencyKnob()
        : GlyphKnob("FREQ")
    {
        setRange(20.0, 20000.0);
        setSkewFactorFromMidPoint(1000.0);
        setValueSuffix(" Hz");
        setDecimalPlaces(0);
        setGlyph(Glyphs::cutoffSymbol);
    }
    
    juce::String FrequencyKnob::getTextFromValue(double value)
    {
        if (value >= 1000.0)
            return juce::String(value / 1000.0, 1) + " kHz";
        else
            return juce::String((int)value) + " Hz";
    }
    
    double FrequencyKnob::getValueFromText(const juce::String& text)
    {
        juce::String cleanText = text.trimEnd().toLowerCase();
        
        if (cleanText.endsWith("khz") || cleanText.endsWith("k"))
        {
            cleanText = cleanText.removeCharacters("khz").trim();
            return cleanText.getDoubleValue() * 1000.0;
        }
        else
        {
            cleanText = cleanText.removeCharacters("hz").trim();
            return cleanText.getDoubleValue();
        }
    }
    
    // ResonanceKnob implementation
    ResonanceKnob::ResonanceKnob()
        : GlyphKnob("RES")
    {
        setRange(0.0, 1.0);
        setValueSuffix("");
        setDecimalPlaces(2);
        setGlyph(Glyphs::resonanceSymbol);
    }
    
    juce::String ResonanceKnob::getTextFromValue(double value)
    {
        return juce::String((int)(value * 100)) + "%";
    }
    
    // MorphKnob implementation
    MorphKnob::MorphKnob()
        : GlyphKnob("MORPH")
    {
        setRange(0.0, 1.0);
        setValueSuffix("");
        setDecimalPlaces(0);
        setGlyph(Glyphs::morphSymbol);
    }
    
    void MorphKnob::setMorphLabels(const juce::String& startLabel, const juce::String& endLabel)
    {
        morphStartLabel = startLabel;
        morphEndLabel = endLabel;
    }
    
    juce::String MorphKnob::getTextFromValue(double value)
    {
        if (value < 0.1)
            return morphStartLabel;
        else if (value > 0.9)
            return morphEndLabel;
        else
        {
            int percent = (int)(value * 100);
            return juce::String(percent) + "%";
        }
    }
}
