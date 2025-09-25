#include "MorphFilter.h"
#include <cmath>

void MorphFilter::prepare(double sampleRate_, int /*maxBlockSize*/)
{
    sampleRate = sampleRate_;
    for (auto& state : channelStates) { state.z1 = 0.0f; state.z2 = 0.0f; }
    
    morph.reset(sampleRate, 0.05);
    cutoff.reset(sampleRate, 0.05);
    resonance.reset(sampleRate, 0.05);
    drive.reset(sampleRate, 0.05);
    
    updateCoefficients();
}

void MorphFilter::reset()
{
    for (auto& state : channelStates) { state.z1 = 0.0f; state.z2 = 0.0f; }
}

void MorphFilter::process(juce::AudioBuffer<float>& buffer)
{
    const int numChannels = juce::jmin(buffer.getNumChannels(), 2);
    const int numSamples = buffer.getNumSamples();
    updateCoefficients();

    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);
        for (int sample = 0; sample < numSamples; ++sample)
            channelData[sample] = processSample(channelData[sample], channel);
    }
}

void MorphFilter::updateCoefficients()
{
    float currentMorph = morph.getNextValue();
    float currentCutoff = cutoff.getNextValue();
    float currentResonance = resonance.getNextValue();
    float currentDrive = drive.getNextValue();

    float normalizedFreq = currentCutoff / static_cast<float>(sampleRate * 0.5);
    normalizedFreq = juce::jlimit(0.001f, 0.99f, normalizedFreq);

    float g = std::tan(juce::MathConstants<float>::pi * normalizedFreq);
    float k = 2.0f - (2.0f * currentResonance);

    coeffs.g = g;
    coeffs.k = k;
    coeffs.a1 = 1.0f / (1.0f + g * (g + k));
    coeffs.a2 = g * coeffs.a1;
    coeffs.a3 = g * coeffs.a2;
}

float MorphFilter::processSample(float input, int channel)
{
    if (channel >= 2) return input;

    auto& state = channelStates[channel];

    float drivenInput = preMode ? input * (1.0f + drive.getCurrentValue()) : input;

    float g = coeffs.g;
    float k = coeffs.k;
    float a1 = coeffs.a1;
    float a2 = coeffs.a2;

    float v1 = a1 * (drivenInput - state.z2);
    float v2 = state.z1 + a2 * v1;
    float v3 = state.z2 + a2 * v1;

    state.z1 += 2.0f * a2 * v1;
    state.z2 += 2.0f * a2 * (v2 - state.z2);

    float lowpass = v3;
    float bandpass = v2;
    float highpass = drivenInput - k * v2 - v3;

    float output = blendResponses(lowpass, bandpass, highpass);

    if (!preMode)
        output *= (1.0f + drive.getCurrentValue());

    return std::tanh(output);
}

float MorphFilter::blendResponses(float lowpass, float bandpass, float highpass) const
{
    float currentMorph = morph.getCurrentValue();
    if (currentMorph < 0.5f)
    {
        float blend = currentMorph * 2.0f;
        return lowpass * (1.0f - blend) + bandpass * blend;
    }
    
    float blend = (currentMorph - 0.5f) * 2.0f;
    return bandpass * (1.0f - blend) + highpass * blend;
}
