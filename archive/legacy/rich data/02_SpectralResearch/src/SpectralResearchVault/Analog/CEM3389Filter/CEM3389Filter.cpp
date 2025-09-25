/******************************************************************************
 * File: CEM3389Filter.cpp 
 * Description: Implementation of secret E-mu Audity CEM3389 filter
 * 
 * The "secret sauce" that makes SpectralCanvas magical. Users never know
 * this filter exists - they just hear incredible sound quality.
 * 
 * Copyright (c) 2025 Spectral Audio Systems
 ******************************************************************************/

#include "CEM3389Filter.h"

//==============================================================================
// Constructor
//==============================================================================

CEM3389Filter::CEM3389Filter()
{
    // Initialize with musical defaults
    setCutoff(1000.0f);        // 1kHz - musical sweet spot
    setResonance(0.3f);        // Subtle character, not harsh
    setSaturation(0.2f);       // Gentle analog warmth
    
    // Setup automatic modulation for "living" sound
    setAutoModulation(true);
    setModulationRate(0.3f);   // Very slow drift
    setModulationDepth(0.1f);  // Subtle movement
    
    reset();
}

//==============================================================================
// Audio Processing Lifecycle
//==============================================================================

void CEM3389Filter::setSampleRate(double sampleRate)
{
    currentSampleRate = sampleRate;
    reset();
    updateFilterCoefficients();
}

void CEM3389Filter::reset()
{
    // Clear all filter state
    for (int ch = 0; ch < MAX_CHANNELS; ++ch)
    {
        x1[ch] = x2[ch] = 0.0f;
        y1[ch] = y2[ch] = 0.0f;
    }
    
    modulationPhase = 0.0f;
    noiseGenerator.setSeedRandomly();
}

void CEM3389Filter::processBlock(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    
    // Update auto-modulation for living sound
    updateAutoModulation();
    updateFilterCoefficients();
    
    // Process each channel
    for (int channel = 0; channel < numChannels && channel < MAX_CHANNELS; ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);
        
        for (int sample = 0; sample < numSamples; ++sample)
        {
            channelData[sample] = processSample(channelData[sample], channel);
        }
    }
}

float CEM3389Filter::processSample(float input, int channel)
{
    if (channel >= MAX_CHANNELS) return input;
    
    //==============================================================================
    // Stage 1: Pre-filter saturation (analog input stage character)
    //==============================================================================
    
    float processedInput = applySaturation(input, saturationAmount.load() * 0.5f);
    
    //==============================================================================
    // Stage 2: CEM3389 Filter Processing
    //==============================================================================
    
    // Calculate dynamic resonance based on input level
    float currentResonance = calculateNonLinearResonance(resonanceAmount.load(), processedInput);
    
    // Apply biquad filter (CEM3389 lowpass characteristic)
    float filteredOutput = b0 * processedInput + b1 * x1[channel] + b2 * x2[channel]
                          - a1 * y1[channel] - a2 * y2[channel];
    
    // Update delay lines
    x2[channel] = x1[channel];
    x1[channel] = processedInput;
    y2[channel] = y1[channel];
    y1[channel] = filteredOutput;
    
    //==============================================================================
    // Stage 3: Post-filter character (CEM3389 output stage)
    //==============================================================================
    
    float characterizedOutput = applyFilterCharacter(filteredOutput, processedInput);
    
    //==============================================================================
    // Stage 4: Final analog character
    //==============================================================================
    
    float finalOutput = applySaturation(characterizedOutput, saturationAmount.load() * 0.3f);
    finalOutput = applyAnalogNoise(finalOutput);
    
    return finalOutput;
}

//==============================================================================
// Parameter Control (Internal Only)
//==============================================================================

void CEM3389Filter::setCutoff(float cutoffHz)
{
    cutoffFreq.store(juce::jlimit(20.0f, 20000.0f, cutoffHz));
}

void CEM3389Filter::setResonance(float resonance)
{
    resonanceAmount.store(juce::jlimit(0.0f, 1.0f, resonance));
}

void CEM3389Filter::setSaturation(float saturation)
{
    saturationAmount.store(juce::jlimit(0.0f, 1.0f, saturation));
}

//==============================================================================
// Automatic Modulation (The Living Sound)
//==============================================================================

void CEM3389Filter::setAutoModulation(bool enabled)
{
    autoModulationEnabled = enabled;
}

void CEM3389Filter::setModulationRate(float rateHz)
{
    modulationRate = juce::jlimit(0.01f, 5.0f, rateHz);
}

void CEM3389Filter::setModulationDepth(float depth)
{
    modulationDepth = juce::jlimit(0.0f, 0.5f, depth);
}

//==============================================================================
// Paint Integration
//==============================================================================

void CEM3389Filter::updateFromPaintData(float pressure, float velocity, juce::Colour color)
{
    // Paint pressure subtly affects filter resonance
    paintPressureInfluence = pressure * 0.2f;  // Very subtle influence
    
    // Paint velocity affects filter response speed
    paintVelocityInfluence = velocity * 0.1f;
    
    // Paint color affects filter character
    lastPaintColor = color;
    
    // Hue affects cutoff frequency slightly
    float hue = color.getHue();
    float baseCutoff = cutoffFreq.load();
    float colorInfluence = (hue - 0.5f) * 200.0f;  // ±200Hz based on color
    setCutoff(baseCutoff + colorInfluence);
    
    // Saturation affects filter resonance
    float colorSat = color.getSaturation();
    float baseResonance = resonanceAmount.load();
    setResonance(baseResonance + colorSat * 0.1f);
}

//==============================================================================
// Internal Processing Methods
//==============================================================================

void CEM3389Filter::updateFilterCoefficients()
{
    float freq = cutoffFreq.load();
    float q = resonanceAmount.load();
    
    // Apply auto-modulation for living sound
    if (autoModulationEnabled)
    {
        float modulation = std::sin(modulationPhase) * modulationDepth;
        freq *= (1.0f + modulation);
    }
    
    // Clamp frequency to valid range
    freq = juce::jlimit(20.0f, static_cast<float>(currentSampleRate * 0.45), freq);
    
    // Calculate biquad coefficients for CEM3389-style lowpass
    float omega = 2.0f * juce::MathConstants<float>::pi * freq / static_cast<float>(currentSampleRate);
    float cosOmega = std::cos(omega);
    float sinOmega = std::sin(omega);
    
    // CEM3389 has a specific Q response curve
    float actualQ = 0.5f + q * 8.0f;  // 0.5 to 8.5 Q range
    float alpha = sinOmega / (2.0f * actualQ);
    
    // Lowpass biquad coefficients
    float a0_temp = 1.0f + alpha;
    a0 = 1.0f;
    a1 = (-2.0f * cosOmega) / a0_temp;
    a2 = (1.0f - alpha) / a0_temp;
    
    b0 = ((1.0f - cosOmega) / 2.0f) / a0_temp;
    b1 = (1.0f - cosOmega) / a0_temp;
    b2 = ((1.0f - cosOmega) / 2.0f) / a0_temp;
}

float CEM3389Filter::applySaturation(float input, float amount)
{
    if (amount <= 0.0f) return input;
    
    // Tube-style saturation (gentle)
    float drive = 1.0f + amount * 3.0f;
    float driven = input * drive;
    
    // Soft clipping with harmonic character
    float saturated = std::tanh(driven * 0.7f) * 1.4f;
    
    // Mix with original for subtlety
    return input + (saturated - input) * amount;
}

float CEM3389Filter::applyAnalogNoise(float input)
{
    // Very quiet analog noise for realism
    float noise = (noiseGenerator.nextFloat() * 2.0f - 1.0f) * analogNoise * 0.001f;
    return input + noise;
}

void CEM3389Filter::updateAutoModulation()
{
    if (!autoModulationEnabled) return;
    
    // Update LFO phase for cutoff modulation
    float phaseIncrement = modulationRate * 2.0f * juce::MathConstants<float>::pi / static_cast<float>(currentSampleRate);
    modulationPhase += phaseIncrement;
    
    if (modulationPhase >= 2.0f * juce::MathConstants<float>::pi)
        modulationPhase -= 2.0f * juce::MathConstants<float>::pi;
}

//==============================================================================
// CEM3389-Specific Character
//==============================================================================

float CEM3389Filter::calculateNonLinearResonance(float baseResonance, float input)
{
    // CEM3389 resonance increases with input level (creates character)
    float inputLevel = std::abs(input);
    float nonLinearBoost = inputLevel * 0.2f;  // Subtle boost
    
    return juce::jlimit(0.0f, 1.0f, baseResonance + nonLinearBoost);
}

float CEM3389Filter::applyFilterCharacter(float filteredSample, float input)
{
    // CEM3389 has distinctive output stage character
    float character = filteredSample;
    
    // Subtle harmonic enhancement
    float harmonics = std::sin(filteredSample * 3.14159f) * 0.05f;
    character += harmonics;
    
    // Gentle compression for analog feel
    character = std::tanh(character * 0.9f) * 1.1f;
    
    return character;
}