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
    // Transposed Direct Form II biquad (TDF-II) - superior numerical stability per DSP research
    float output = b0 * input + z1;
    z1 = b1 * input - a1 * output + z2;
    z2 = b2 * input - a2 * output;

    // Per-section saturation for EMU character
    if (saturationAmount > 0.0f)
    {
        output = std::tanh(output * (1.0f + saturationAmount * 2.0f)) / (1.0f + saturationAmount);
    }

    // Denormal protection
    constexpr float denormEps = 1.0e-20f;
    if (std::abs(z1) < denormEps) z1 = 0.0f;
    if (std::abs(z2) < denormEps) z2 = 0.0f;
    if (std::abs(output) < denormEps) output = 0.0f;

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

void AuthenticEMUZPlane::process (juce::AudioBuffer<float>& buffer)
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    // Ensure we have enough channel states
    if (channelStates.size() != (size_t) numChannels)
    {
        channelStates.resize((size_t) numChannels);
        for (auto& channelState : channelStates)
            channelState = filterSections;  // Copy the current sections as starting point
    }

    // Update coefficients once per block for all channels
    updateCoefficientsBlock();

    // Process each channel with its own state
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* samples = buffer.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i)
        {
            float wet = samples[i] * currentDrive;
            for (auto& section : channelStates[(size_t) ch])
                wet = section.processSample(wet, sectionSaturation);
            samples[i] = wet;
        }
    }
}

void AuthenticEMUZPlane::getSectionCoeffs (std::array<BiquadCoeffs, 6>& dest) const
{
    for (size_t i = 0; i < 6; ++i)
    {
        dest[i].b0 = filterSections[i].b0;
        dest[i].b1 = filterSections[i].b1;
        dest[i].b2 = filterSections[i].b2;
        dest[i].a1 = filterSections[i].a1;
        dest[i].a2 = filterSections[i].a2;
    }
}

void AuthenticEMUZPlane::setLFOPhase(float phase)
{
    lfoPhase = phase;
}
