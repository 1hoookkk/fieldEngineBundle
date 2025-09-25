/******************************************************************************
 * File: CEM3389Filter.h
 * Description: Secret E-mu Audity CEM3389 filter emulation (INVISIBLE TO USER)
 * 
 * This filter provides the "magical" character that makes SpectralCanvas special.
 * Users never see it - they just hear amazing sound quality and think it's
 * great sound design. This is the "secret sauce" that makes the plugin 
 * gatekeep-worthy.
 * 
 * Based on CEM3389 filter chip characteristics:
 * - Non-linear resonance behavior
 * - Subtle saturation and warmth
 * - Self-oscillation at high resonance
 * - Gritty analog character
 * 
 * Copyright (c) 2025 Spectral Audio Systems
 ******************************************************************************/

#pragma once
#include <JuceHeader.h>
#include <cmath>

/**
 * @brief Secret E-mu Audity CEM3389 filter emulation
 * 
 * This filter is NEVER exposed to the user interface. It's automatically
 * applied to all audio output to give SpectralCanvas its magical character.
 * Users just think "this plugin sounds amazing" without knowing why.
 */
class CEM3389Filter
{
public:
    CEM3389Filter();
    ~CEM3389Filter() = default;
    
    //==============================================================================
    // Audio Processing
    
    void setSampleRate(double sampleRate);
    void reset();
    void processBlock(juce::AudioBuffer<float>& buffer);
    float processSample(float input, int channel = 0);
    
    //==============================================================================
    // Filter Parameters (INTERNAL ONLY - No User Control)
    
    void setCutoff(float cutoffHz);        // 20Hz - 20kHz
    void setResonance(float resonance);    // 0.0 - 1.0 (can self-oscillate at 1.0)
    void setSaturation(float saturation);  // 0.0 - 1.0 (analog warmth)
    
    //==============================================================================
    // Automatic Modulation (The Living Sound)
    
    void setAutoModulation(bool enabled);
    void setModulationRate(float rateHz);   // 0.01 - 5.0 Hz
    void setModulationDepth(float depth);   // 0.0 - 0.5
    
    //==============================================================================
    // Paint Integration
    
    void updateFromPaintData(float pressure, float velocity, juce::Colour color);
    
private:
    //==============================================================================
    // Internal Processing Methods
    
    void updateFilterCoefficients();
    float applySaturation(float input, float amount);
    float applyAnalogNoise(float input);
    void updateAutoModulation();
    
    //==============================================================================
    // CEM3389-Specific Character
    
    float calculateNonLinearResonance(float baseResonance, float input);
    float applyFilterCharacter(float filteredSample, float input);
    
    //==============================================================================
    // State Variables
    
    static constexpr int MAX_CHANNELS = 8;
    
    // Filter state per channel
    float x1[MAX_CHANNELS] = {0};
    float x2[MAX_CHANNELS] = {0};
    float y1[MAX_CHANNELS] = {0};
    float y2[MAX_CHANNELS] = {0};
    
    // Biquad coefficients
    float b0 = 0.0f, b1 = 0.0f, b2 = 0.0f;
    float a0 = 1.0f, a1 = 0.0f, a2 = 0.0f;
    
    // Parameters (atomic for thread safety)
    std::atomic<float> cutoffFreq{1000.0f};
    std::atomic<float> resonanceAmount{0.3f};
    std::atomic<float> saturationAmount{0.2f};
    
    // Auto-modulation
    bool autoModulationEnabled = true;
    float modulationRate = 0.3f;
    float modulationDepth = 0.1f;
    float modulationPhase = 0.0f;
    
    // Paint interaction
    float paintPressureInfluence = 0.0f;
    float paintVelocityInfluence = 0.0f;
    juce::Colour lastPaintColor = juce::Colours::transparentBlack;
    
    // Internal state
    double currentSampleRate = 44100.0;
    float analogNoise = 0.1f;
    juce::Random noiseGenerator;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CEM3389Filter)
};