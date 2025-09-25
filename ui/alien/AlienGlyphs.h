#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

namespace AlienUI
{
    namespace Glyphs
    {
        // Alien symbols for parameters (Unicode private use area)
        const juce::String cutoffSymbol      { juce::CharPointer_UTF8("\uE100") };  // Frequency wave
        const juce::String resonanceSymbol   { juce::CharPointer_UTF8("\uE101") };  // Energy ring
        const juce::String morphSymbol       { juce::CharPointer_UTF8("\uE102") };  // Transform glyph
        const juce::String driveSymbol       { juce::CharPointer_UTF8("\uE103") };  // Power surge
        const juce::String mixSymbol         { juce::CharPointer_UTF8("\uE104") };  // Blend orb
        const juce::String feedbackSymbol    { juce::CharPointer_UTF8("\uE105") };  // Recursion spiral
        const juce::String phaseSymbol       { juce::CharPointer_UTF8("\uE106") };  // Phase shift
        const juce::String lfoSymbol         { juce::CharPointer_UTF8("\uE107") };  // Oscillation
        
        // Z-plane filter type glyphs
        const juce::String lowpassGlyph     { juce::CharPointer_UTF8("\uE110") };
        const juce::String highpassGlyph    { juce::CharPointer_UTF8("\uE111") };
        const juce::String bandpassGlyph    { juce::CharPointer_UTF8("\uE112") };
        const juce::String notchGlyph       { juce::CharPointer_UTF8("\uE113") };
        const juce::String combGlyph        { juce::CharPointer_UTF8("\uE114") };
        const juce::String allpassGlyph     { juce::CharPointer_UTF8("\uE115") };
        
        // Energy level indicators
        const juce::String energyEmpty      { juce::CharPointer_UTF8("\uE120") };
        const juce::String energyLow        { juce::CharPointer_UTF8("\uE121") };
        const juce::String energyMid        { juce::CharPointer_UTF8("\uE122") };
        const juce::String energyHigh       { juce::CharPointer_UTF8("\uE123") };
        const juce::String energyOverload   { juce::CharPointer_UTF8("\uE124") };
        
        // Alternative ASCII representations for fallback
        namespace ASCII
        {
            const juce::String cutoff      { "[~]" };
            const juce::String resonance   { "(@)" };
            const juce::String morph       { "<>" };
            const juce::String drive       { "/^\\" };
            const juce::String mix         { "(*)" };
            const juce::String feedback    { "@>" };
            const juce::String phase       { "~|~" };
            const juce::String lfo         { "~~~" };
        }
        
        // Font creation helpers
        inline juce::Font createAlienFont(float height)
        {
            // Try to load custom alien font first
            static const juce::Font alienFont = []() {
                // In production, this would load an actual alien-styled font
                // For now, use a monospace font with modifications
                juce::Font font("Consolas", 14.0f, juce::Font::plain);
                font.setExtraKerningFactor(0.15f);  // Wider spacing for alien feel
                return font;
            }();
            
            return alienFont.withHeight(height);
        }
        
        inline juce::Font createGlyphFont(float height)
        {
            // Special font for rendering glyphs
            return juce::Font("Segoe UI Symbol", height, juce::Font::plain);
        }
        
        // Draw alien text with glow effect
        inline void drawAlienText(juce::Graphics& g, const juce::String& text,
                                 juce::Rectangle<float> bounds,
                                 juce::Justification justification,
                                 juce::Colour glowColor)
        {
            // Draw glow
            g.setColour(glowColor.withAlpha(0.3f));
            g.setFont(createAlienFont(bounds.getHeight() * 0.7f));
            
            for (int i = 0; i < 3; ++i)
            {
                auto blurBounds = bounds.expanded(2.0f * (i + 1));
                g.drawText(text, blurBounds, justification, false);
            }
            
            // Draw main text
            g.setColour(juce::Colours::white);
            g.drawText(text, bounds, justification, false);
        }
        
        // Parameter name translations
        inline juce::String getParameterGlyph(const juce::String& paramName)
        {
            if (paramName.containsIgnoreCase("cutoff") || paramName.containsIgnoreCase("freq"))
                return cutoffSymbol;
            if (paramName.containsIgnoreCase("resonance") || paramName.containsIgnoreCase("q"))
                return resonanceSymbol;
            if (paramName.containsIgnoreCase("morph"))
                return morphSymbol;
            if (paramName.containsIgnoreCase("drive"))
                return driveSymbol;
            if (paramName.containsIgnoreCase("mix"))
                return mixSymbol;
            if (paramName.containsIgnoreCase("feedback"))
                return feedbackSymbol;
            if (paramName.containsIgnoreCase("phase"))
                return phaseSymbol;
            if (paramName.containsIgnoreCase("lfo"))
                return lfoSymbol;
                
            return "?";  // Unknown parameter
        }
    }
}
