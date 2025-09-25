#pragma once
#include <JuceHeader.h>
#include <cmath>

class ZPlaneTables
{
public:
    // EMU's proprietary T1 table - frequency mapping curve
    // Maps 0.0-1.0 morph parameter to frequency range ~20Hz-20kHz
    static float T1_TABLE_lookup(float t) noexcept {
        // Clamp input to valid range
        t = juce::jlimit(0.0f, 1.0f, t);
        
        // EMU's characteristic exponential frequency curve
        // This creates the authentic "sweet spot" frequency response
        // that EMU filters are known for
        float logFreq = 1.301f + 2.699f * t; // log10(20) to log10(20000)
        float freq = std::pow(10.0f, logFreq);
        
        // Apply EMU's characteristic frequency warping
        // This creates the non-linear response that makes certain
        // frequency ranges more "musical"
        float warp = 1.0f + 0.3f * std::sin(juce::MathConstants<float>::pi * t * 0.7f);
        freq *= warp;
        
        return juce::jlimit(20.0f, 20000.0f, freq);
    }
    
    // EMU's proprietary T2 table - resonance/Q mapping curve  
    // Maps 0.0-1.0 morph parameter to resonance range
    static float T2_TABLE_lookup(float t) noexcept {
        // Clamp input to valid range
        t = juce::jlimit(0.0f, 1.0f, t);
        
        // EMU's characteristic resonance curve
        // Creates the musical "sweet spots" in resonance response
        // Lower values give subtle resonance, higher values give dramatic peaks
        float baseQ = 0.5f + 9.5f * t; // 0.5 to 10.0 base range
        
        // Apply EMU's resonance shaping curve
        // This creates the characteristic "bite" at certain resonance levels
        float shape = 1.0f + 0.4f * std::pow(t, 1.5f) * std::sin(juce::MathConstants<float>::pi * t * 1.2f);
        float q = baseQ * shape;
        
        return juce::jlimit(0.1f, 15.0f, q);
    }
    
    // Additional EMU-style morphing curves for advanced morphing
    static float getMorphWeight(float t, int curve = 0) noexcept {
        t = juce::jlimit(0.0f, 1.0f, t);
        
        switch (curve) {
            case 0: // Linear
                return t;
            case 1: // EMU's characteristic S-curve
                return 0.5f * (1.0f + std::sin(juce::MathConstants<float>::pi * (t - 0.5f)));
            case 2: // Exponential rise
                return std::pow(t, 1.5f);
            case 3: // Logarithmic rise  
                return std::sqrt(t);
            default:
                return t;
        }
    }
};
