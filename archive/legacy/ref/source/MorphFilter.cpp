#include "MorphFilter.h"
#include <cmath>

void MorphFilter::prepare(double sampleRate, int maxBlockSize)
{
    this->sampleRate = sampleRate;
    
    // Reset channel states
    for (auto& state : channelStates)
    {
        state.z1 = 0.0f;
        state.z2 = 0.0f;
    }
    
    updateCoefficients();
}

void MorphFilter::reset()
{
    for (auto& state : channelStates)
    {
        state.z1 = 0.0f;
        state.z2 = 0.0f;
    }
}

void MorphFilter::process(juce::AudioBuffer<float>& buffer)
{
    const int numChannels = juce::jmin(buffer.getNumChannels(), 2);
    const int numSamples = buffer.getNumSamples();
    
    // Update coefficients for current parameters
    updateCoefficients();
    
    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);
        
        for (int sample = 0; sample < numSamples; ++sample)
        {
            channelData[sample] = processSample(channelData[sample], channel);
        }
    }
}

void MorphFilter::updateCoefficients()
{
    // Smooth parameter changes
    currentMorph += (targetMorph - currentMorph) * smoothingFactor;
    currentCutoff += (targetCutoff - currentCutoff) * smoothingFactor;
    currentResonance += (targetResonance - currentResonance) * smoothingFactor;
    currentDrive += (targetDrive - targetDrive) * smoothingFactor;
    
    // Calculate normalized frequency (0-1)
    float normalizedFreq = currentCutoff / static_cast<float>(sampleRate * 0.5);
    normalizedFreq = juce::jlimit(0.001f, 0.99f, normalizedFreq);
    
    // Pre-calculate common coefficients for State Variable Filter
    float g = std::tan(3.14159f * normalizedFreq);
    float k = 2.0f - (2.0f * currentResonance);
    
    // Store coefficients for all response types
    for (int i = 0; i < NUM_RESPONSES; ++i)
    {
        responses[i].g = g;
        responses[i].k = k;
        responses[i].a1 = 1.0f / (1.0f + g * (g + k));
        responses[i].a2 = g * responses[i].a1;
        responses[i].a3 = g * responses[i].a2;
    }
}

float MorphFilter::processSample(float input, int channel)
{
    if (channel >= 2) return input;
    
    auto& state = channelStates[channel];
    
    // Apply drive (pre or post)
    float drivenInput = preMode ? input * (1.0f + currentDrive) : input;
    
    // State Variable Filter processing
    // This is a simplified implementation - interpolate between LP and HP
    float g = responses[0].g;
    float k = responses[0].k;
    float a1 = responses[0].a1;
    float a2 = responses[0].a2;
    
    float v1 = a1 * (drivenInput - state.z2);
    float v2 = state.z1 + a2 * v1;
    float v3 = state.z2 + a2 * v1;
    
    state.z1 += 2.0f * a2 * v1;
    state.z2 += 2.0f * a2 * (v2 - state.z2);
    
    // Morph between filter responses
    float lowpass = v3;
    float bandpass = v2;
    float highpass = drivenInput - k * v2 - v3;
    
    // Simple morphing: 0.0 = LP, 0.5 = BP, 1.0 = HP
    float output;
    if (currentMorph < 0.5f)
    {
        float blend = currentMorph * 2.0f;
        output = lowpass * (1.0f - blend) + bandpass * blend;
    }
    else
    {
        float blend = (currentMorph - 0.5f) * 2.0f;
        output = bandpass * (1.0f - blend) + highpass * blend;
    }
    
    // Apply drive (post)
    if (!preMode)
    {
        output *= (1.0f + currentDrive);
    }
    
    // Soft clipping
    output = std::tanh(output);
    
    return output;
}
