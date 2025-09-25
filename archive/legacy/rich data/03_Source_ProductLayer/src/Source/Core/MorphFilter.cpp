#include "MorphFilter.h"
#include <cmath>

void MorphFilter::prepare(double sampleRate, int maxBlockSize)
{
    this->sampleRate = sampleRate;
    
    // Calculate smoothing coefficient for ~5ms attack
    const float attackTime = 0.005f;
    smoothingCoeff = std::exp(-1.0f / (attackTime * sampleRate));
    fastSmoothingCoeff = std::exp(-1.0f / (0.001f * sampleRate));
    
    // Build gain compensation LUT
    for (int i = 0; i < 256; ++i)
    {
        float resonance = (i / 255.0f) * 10.0f;
        // Empirical gain compensation formula
        gainCompLUT[i] = 1.0f / (1.0f + resonance * 0.15f);
    }
    
    reset();
    calculateCoefficients();
}

void MorphFilter::reset()
{
    for (auto& state : channelStates)
    {
        state.z1 = 0.0f;
        state.z2 = 0.0f;
    }
    
    currentMorph = targetMorph;
    currentCutoff = targetCutoff;
    currentResonance = targetResonance;
    currentDrive = targetDrive;
}

void MorphFilter::process(juce::AudioBuffer<float>& buffer)
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    
    if (numChannels == 0 || numSamples == 0)
        return;
    
    // Update smoothed parameters
    updateSmoothing();
    
    // Recalculate coefficients if needed
    if (std::abs(currentCutoff - targetCutoff) > 1.0f || 
        std::abs(currentResonance - targetResonance) > 0.01f)
    {
        calculateCoefficients();
    }
    
    // Get drive gain
    const float driveGain = juce::Decibels::decibelsToGain(currentDrive);
    
    // Get resonance gain compensation
    const int resIndex = juce::jlimit(0, 255, (int)(currentResonance * 25.5f));
    const float gainComp = gainCompLUT[resIndex];
    
    // Process each channel
    for (int ch = 0; ch < numChannels && ch < 2; ++ch)
    {
        float* channelData = buffer.getWritePointer(ch);
        SVFState& state = channelStates[ch];
        
        // Get morphed coefficients
        const int morph0 = (int)currentMorph;
        const int morph1 = juce::jmin(morph0 + 1, (int)NUM_RESPONSES - 1);
        const float morphFrac = currentMorph - morph0;
        
        const Coefficients& c0 = responses[morph0];
        const Coefficients& c1 = responses[morph1];
        
        // Interpolate coefficients
        const float g = lerp(c0.g, c1.g, morphFrac);
        const float k = lerp(c0.k, c1.k, morphFrac);
        const float a1 = lerp(c0.a1, c1.a1, morphFrac);
        const float a2 = lerp(c0.a2, c1.a2, morphFrac);
        const float a3 = lerp(c0.a3, c1.a3, morphFrac);
        
        for (int i = 0; i < numSamples; ++i)
        {
            float input = channelData[i];
            
            // Pre-drive
            if (preMode)
            {
                input *= driveGain;
                input = fastTanh(input);
            }
            
            // State Variable Filter processing
            const float v3 = input - state.z2 * k;
            const float v1 = a1 * state.z1 + a2 * v3;
            const float v2 = state.z2 + a3 * state.z1;
            
            state.z1 = 2.0f * v1 - state.z1;
            state.z2 = 2.0f * v2 - state.z2;
            
            // Output mixing based on filter type
            float output = 0.0f;
            const int responseType = morph0;
            
            switch (responseType)
            {
                case LP: output = v2; break;
                case BP: output = v1; break;
                case HP: output = v3; break;
                case NOTCH: output = input - v1 * k; break;
                case VOWEL: output = v1 * 0.7f + v2 * 0.3f; break;
                default: output = v2; break;
            }
            
            // Apply gain compensation
            output *= gainComp;
            
            // Post-drive
            if (!preMode)
            {
                output *= driveGain;
                output = fastTanh(output);
            }
            
            channelData[i] = output;
        }
    }
}

void MorphFilter::calculateCoefficients()
{
    // Convert cutoff to normalized frequency
    const float w = currentCutoff / sampleRate;
    const float wc = std::tan(juce::MathConstants<float>::pi * juce::jlimit(0.0001f, 0.4999f, w));
    
    // Calculate SVF coefficients
    const float g = wc / (1.0f + wc);
    const float k = 1.0f / currentResonance;
    
    // Pre-calculate response coefficients
    for (int i = 0; i < NUM_RESPONSES; ++i)
    {
        Coefficients& c = responses[i];
        c.g = g;
        c.k = k;
        
        // Common coefficients
        c.a1 = 1.0f / (1.0f + g * (g + k));
        c.a2 = g * c.a1;
        c.a3 = g * c.a2;
    }
}

void MorphFilter::updateSmoothing()
{
    // Smooth morph parameter (slower)
    currentMorph = smoothingCoeff * currentMorph + (1.0f - smoothingCoeff) * targetMorph;
    
    // Smooth frequency parameters (faster to prevent zippering)
    currentCutoff = fastSmoothingCoeff * currentCutoff + (1.0f - fastSmoothingCoeff) * targetCutoff;
    currentResonance = fastSmoothingCoeff * currentResonance + (1.0f - fastSmoothingCoeff) * targetResonance;
    
    // Smooth drive
    currentDrive = smoothingCoeff * currentDrive + (1.0f - smoothingCoeff) * targetDrive;
}