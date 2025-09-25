#include "EMUFilter.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
    // Anonymous namespace for constants to limit their scope to this file.
    constexpr float DefaultLFORate = 1.2f;
    constexpr float DefaultLFODepth = 0.15f;
    constexpr float DefaultEnvDepth = 0.35f;
    constexpr float DefaultIntensity = 0.4f;
    constexpr float DefaultSectionSaturation = 0.2f;
}

AuthenticEMUZPlane::AuthenticEMUZPlane()
{
    // Set default values using the named constants.
    lfoRate = DefaultLFORate;
    lfoDepth = DefaultLFODepth;
    envDepth = DefaultEnvDepth;
    currentIntensity = DefaultIntensity;
    sectionSaturation = DefaultSectionSaturation;
}

float AuthenticEMUZPlane::BiquadSection::processSample(float input, float saturationAmount)
{
    // Direct Form II biquad
    float w = input - a1 * z1 - a2 * z2;
    float output = b0 * w + b1 * z1 + b2 * z2;

    // Per-section saturation for EMU character
    if (saturationAmount > 0.0f)
    {
        output = std::tanh(output * (1.0f + saturationAmount * 2.0f)) / (1.0f + saturationAmount);
    }

    z2 = z1;
    z1 = w;
    return output;
}

void AuthenticEMUZPlane::prepareToPlay(double sampleRate)
{
    this->sampleRate = sampleRate;
    morphSmoother.reset(sampleRate, 0.05); // 50ms smoothing
    intensitySmoother.reset(sampleRate, 0.05);
    reset();
}

void AuthenticEMUZPlane::reset()
{
    for (auto& section : filterSections)
        section.reset();
    lfoPhase = 0.0f;
    morphSmoother.setCurrentAndTargetValue(currentMorph);
    intensitySmoother.setCurrentAndTargetValue(currentIntensity);
}

float AuthenticEMUZPlane::processSample(float input)
{
    // RT-OPTIMIZED: No coefficient updates per sample!
    float wet = input * currentDrive;
    for (auto& section : filterSections)
        wet = section.processSample(wet, sectionSaturation);
    return wet;
}

void AuthenticEMUZPlane::processBlock(float* samples, int numSamples)
{
    // RT-OPTIMIZED: Update coefficients once per block, not per sample
    updateCoefficientsBlock();
    
    // Process samples with stable coefficients
    for (int i = 0; i < numSamples; ++i)
        samples[i] = processSample(samples[i]);
}

void AuthenticEMUZPlane::setMorphPair(MorphPair pair)
{
    currentPair = pair;
    updateCoefficientsBlock();
}

void AuthenticEMUZPlane::setMorphPosition(float position)
{
    currentMorph = juce::jlimit(0.0f, 1.0f, position);
}

void AuthenticEMUZPlane::setIntensity(float intensity)
{
    currentIntensity = juce::jlimit(0.0f, 1.0f, intensity);
}

void AuthenticEMUZPlane::setDrive(float drive)
{
    currentDrive = juce::Decibels::decibelsToGain(drive);
}

void AuthenticEMUZPlane::setSectionSaturation(float amount)
{
    sectionSaturation = juce::jlimit(0.0f, 1.0f, amount);
}

void AuthenticEMUZPlane::setAutoMakeup(bool enabled)
{
    autoMakeupEnabled = enabled;
}

void AuthenticEMUZPlane::setLFORate(float hz)
{
    lfoRate = juce::jlimit(0.02f, 8.0f, hz);
}

void AuthenticEMUZPlane::setLFODepth(float depth)
{
    lfoDepth = juce::jlimit(0.0f, 1.0f, depth);
}

void AuthenticEMUZPlane::setEnvDepth(float depth)
{
    envDepth = juce::jlimit(0.0f, 1.0f, depth);
}

void AuthenticEMUZPlane::updateCoefficientsBlock()
{
    // RT-OPTIMIZED: Cache previous values to avoid unnecessary recalculation
    static float lastMorph = -1.0f;
    static float lastIntensity = -1.0f;
    static float lastLFOPhase = -1.0f;
    
    // LFO modulation
    lfoPhase += juce::MathConstants<float>::twoPi * lfoRate / sampleRate;
    if (lfoPhase >= juce::MathConstants<float>::twoPi)
        lfoPhase -= juce::MathConstants<float>::twoPi;
    float lfoMod = 0.5f * (1.0f + std::sin(lfoPhase)) * lfoDepth;

    float targetMorph = juce::jlimit(0.0f, 1.0f, currentMorph + lfoMod);
    morphSmoother.setTargetValue(targetMorph);
    intensitySmoother.setTargetValue(currentIntensity);

    float smoothedMorph = morphSmoother.getNextValue();
    float smoothedIntensity = intensitySmoother.getNextValue();
    
    // RT-OPTIMIZED: Only update coefficients if parameters actually changed
    const float epsilon = 0.001f;
    if (std::abs(smoothedMorph - lastMorph) < epsilon && 
        std::abs(smoothedIntensity - lastIntensity) < epsilon)
    {
        return; // No significant change, skip expensive coefficient calculation
    }
    
    lastMorph = smoothedMorph;
    lastIntensity = smoothedIntensity;
    
    const auto& pair = MORPH_PAIRS[currentPair];
    const auto& shapeA = AUTHENTIC_EMU_SHAPES[pair[0]];
    const auto& shapeB = AUTHENTIC_EMU_SHAPES[pair[1]];

    interpolatePoles(shapeA, shapeB, smoothedMorph);

    for (int i = 0; i < 6; ++i)
        poleTosBiquadCoeffs(currentPoles[i], filterSections[i]);
}

void AuthenticEMUZPlane::interpolatePoles(const std::array<float, 12>& shapeA, const std::array<float, 12>& shapeB, float morphPos)
{
    for (int i = 0; i < 6; ++i)
    {
        float rA = shapeA[i * 2];
        float thetaA = shapeA[i * 2 + 1];
        float rB = shapeB[i * 2];
        float thetaB = shapeB[i * 2 + 1];
        currentPoles[i].r = juce::jmap(morphPos, rA, rB);
        currentPoles[i].theta = juce::jmap(morphPos, thetaA, thetaB);
    }
}

void AuthenticEMUZPlane::poleTosBiquadCoeffs(const PolePair& pole, BiquadSection& section)
{
    float r = pole.r * intensitySmoother.getNextValue();
    float theta = pole.theta;

    section.a1 = -2.0f * r * std::cos(theta);
    section.a2 = r * r;
    
    // Simple bandpass coefficients
    section.b0 = (1.0f - r);
    section.b1 = 0.0f;
    section.b2 = -(1.0f - r);

    if (autoMakeupEnabled)
    {
        // This is a simplified makeup gain. A more accurate one would analyze the filter response.
        float makeup = 1.0f / (1.0f - r + 0.1f);
        section.b0 *= makeup;
        section.b2 *= makeup;
    }
}
