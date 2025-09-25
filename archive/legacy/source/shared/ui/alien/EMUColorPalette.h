#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace AlienUI
{
    namespace Colors
    {
        // Primary EMU-inspired colors with alien twist
        const juce::Colour deepSpace        { 0xFF000814 };  // Almost black with blue tint
        const juce::Colour cosmicBlue       { 0xFF00D8FF };  // EMU signature blue
        const juce::Colour plasmaGlow       { 0xFF00FFFF };  // Cyan energy
        const juce::Colour alienPurple      { 0xFF8B00FF };  // Mysterious purple
        const juce::Colour starWhite        { 0xFFE0F7FF };  // Cool white
        
        // Z-plane visualization colors
        const juce::Colour filterPole       { 0xFFFF00AA };  // Hot pink for poles
        const juce::Colour filterZero       { 0xFF00FF88 };  // Green for zeros
        const juce::Colour gridLines        { 0x2000D8FF };  // Faint blue grid
        const juce::Colour resonanceGlow    { 0x80FF00FF };  // Magenta resonance
        
        // Control colors
        const juce::Colour knobTrack        { 0x4000D8FF };  // Semi-transparent blue
        const juce::Colour knobValue        { 0xFFFFFFFF };  // Pure white
        const juce::Colour knobGlow         { 0x6000FFFF };  // Cyan glow
        const juce::Colour textPrimary      { 0xFFE0F7FF };  // Cool white text
        const juce::Colour textSecondary    { 0xB000D8FF };  // Blue text
        
        // Energy states
        const juce::Colour energyLow        { 0xFF0080FF };  // Deep blue
        const juce::Colour energyMid        { 0xFF00D8FF };  // EMU blue
        const juce::Colour energyHigh       { 0xFF00FFFF };  // Bright cyan
        const juce::Colour energyCritical   { 0xFFFF00AA };  // Warning pink
        
        // Background layers
        const juce::Colour bgLayer1         { 0xFF000814 };  // Deep space
        const juce::Colour bgLayer2         { 0xFF001428 };  // Slightly lighter
        const juce::Colour bgLayer3         { 0xFF002142 };  // Control background
        
        // Gradients
        inline juce::ColourGradient createPlasmaGradient(juce::Rectangle<float> bounds)
        {
            juce::ColourGradient gradient(alienPurple, bounds.getTopLeft(),
                                         cosmicBlue, bounds.getBottomRight(), false);
            gradient.addColour(0.5f, plasmaGlow);
            return gradient;
        }
        
        inline juce::ColourGradient createEnergyGradient(juce::Rectangle<float> bounds, float energy)
        {
            auto startColor = energy < 0.33f ? energyLow :
                             energy < 0.66f ? energyMid : energyHigh;
            auto endColor = energy > 0.8f ? energyCritical : energyHigh;
            
            return juce::ColourGradient(startColor, bounds.getCentre(),
                                       endColor, bounds.getBottomRight(), true);
        }
    }
}
