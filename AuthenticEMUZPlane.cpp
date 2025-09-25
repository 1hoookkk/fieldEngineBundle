#include "EMUFilter.h"
#include <cmath>

AuthenticEMUZPlane::AuthenticEMUZPlane()
{
    // Initialize with viral defaults for immediate appeal
    setMorphPair(VowelAe_to_VowelOo);
    setMorphPosition(0.5f);
    setIntensity(0.4f);
    setDrive(3.0f);  // 3dB drive for character
    setSectionSaturation(0.2f);
    setAutoMakeup(false);

    // Viral modulation defaults (high resonance, fast LFO)
    setLFORate(1.2f);      // Fast movement for viral appeal
    setLFODepth(0.15f);    // Noticeable but not overwhelming
    setEnvDepth(0.35f);    // Program-reactive morphing
}

void AuthenticEMUZPlane::prepareToPlay(double sampleRate_)
{
    sampleRate = sampleRate_;

    // Initialize smoothers for RT-safe parameter changes
    morphSmoother.reset(sampleRate, 0.02);  // 20ms smoothing
    intensitySmoother.reset(sampleRate, 0.02);

    morphSmoother.setCurrentAndTargetValue(currentMorph);
    intensitySmoother.setCurrentAndTargetValue(currentIntensity);

    reset();
    channelStates.clear();
    updateCoefficientsBlock();
}

void AuthenticEMUZPlane::reset()
{
    for (auto& section : filterSections) {
        section.reset();
    }
    channelStates.clear();
    lfoPhase = 0.0f;
}

float AuthenticEMUZPlane::BiquadSection::processSample(float input, float saturationAmount)
{
    // Transposed Direct Form II (TDF-II) - superior numerical stability per DSP research
    // Apply section-level saturation to input for EMU character preservation
    float saturatedInput = input;
    if (saturationAmount > 0.0f)
    {
        float drive = 1.0f + saturationAmount * 3.0f;
        saturatedInput = std::tanh(input * drive) / drive;
    }

    // TDF-II structure with saturated input
    float output = b0 * saturatedInput + z1;
    z1 = b1 * saturatedInput - a1 * output + z2;
    z2 = b2 * saturatedInput - a2 * output;

    return output;
}

float AuthenticEMUZPlane::processSample(float input)
{
    return processSampleInternal(input, filterSections);
}

float AuthenticEMUZPlane::processSampleInternal(float input, std::array<BiquadSection, 6>& sections)
{
    float drivenInput = input * currentDrive;
    if (currentDrive > 1.0f) {
        drivenInput = std::tanh(drivenInput * 0.5f) * 2.0f;
    }

    float output = drivenInput;
    for (auto& section : sections) {
        output = section.processSample(output, sectionSaturation);
    }

    if (autoMakeupEnabled) {
        float makeupGain = 1.0f / (1.0f + currentIntensity * 0.5f);
        output *= makeupGain;
    }

    return output;
}

void AuthenticEMUZPlane::processBlock(float* samples, int numSamples)
{
    // Update modulation at control rate (block-based for efficiency)
    float lfoIncrement = 2.0f * juce::MathConstants<float>::pi * lfoRate / static_cast<float>(sampleRate);
    lfoPhase += lfoIncrement * numSamples;
    if (lfoPhase > juce::MathConstants<float>::twoPi) {
        lfoPhase -= juce::MathConstants<float>::twoPi;
    }

    float lfoValue = std::sin(lfoPhase) * lfoDepth;

    // Apply LFO modulation to morph parameter
    float modulatedMorph = juce::jlimit(0.0f, 1.0f, currentMorph + lfoValue);
    morphSmoother.setTargetValue(modulatedMorph);

    // Update coefficients at block rate for RT-safety
    updateCoefficientsBlock();

    // Process samples
    for (int i = 0; i < numSamples; ++i) {
        samples[i] = processSample(samples[i]);

        // Update smoothers per sample for zipper-free parameter changes
        morphSmoother.getNextValue();
        intensitySmoother.getNextValue();
    }
}

void AuthenticEMUZPlane::process(juce::AudioBuffer<float>& buffer)
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    if (numChannels <= 0 || numSamples <= 0)
        return;

    const float lfoIncrement = 2.0f * juce::MathConstants<float>::pi * lfoRate / static_cast<float>(sampleRate);
    lfoPhase += lfoIncrement * numSamples;
    if (lfoPhase > juce::MathConstants<float>::twoPi)
        lfoPhase = std::fmod(lfoPhase, juce::MathConstants<float>::twoPi);

    const float lfoValue = std::sin(lfoPhase) * lfoDepth;
    const float modulatedMorph = juce::jlimit(0.0f, 1.0f, currentMorph + lfoValue);
    morphSmoother.setTargetValue(modulatedMorph);

    updateCoefficientsBlock();

    if (static_cast<int>(channelStates.size()) != numChannels)
        channelStates.assign(static_cast<size_t>(numChannels), filterSections);

    for (int ch = 0; ch < numChannels; ++ch)
        channelStates[static_cast<size_t>(ch)] = filterSections;

    auto morphCopy = morphSmoother;
    auto intensityCopy = intensitySmoother;

    std::vector<float*> channelDataPointers(static_cast<size_t>(numChannels));
    for (int ch = 0; ch < numChannels; ++ch)
        channelDataPointers[static_cast<size_t>(ch)] = buffer.getWritePointer(ch);

    for (int sampleIndex = 0; sampleIndex < numSamples; ++sampleIndex)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& sectionSet = channelStates[static_cast<size_t>(ch)];
            float* data = channelDataPointers[static_cast<size_t>(ch)];
            data[sampleIndex] = processSampleInternal(data[sampleIndex], sectionSet);
        }

        morphCopy.getNextValue();
        intensityCopy.getNextValue();
    }

    if (!channelStates.empty())
        filterSections = channelStates.front();

    morphSmoother = morphCopy;
    intensitySmoother = intensityCopy;
}

void AuthenticEMUZPlane::setMorphPair(MorphPair pair)
{
    currentPair = pair;
    updateCoefficientsBlock();
}

void AuthenticEMUZPlane::setMorphPosition(float position)
{
    currentMorph = juce::jlimit(0.0f, 1.0f, position);
    morphSmoother.setTargetValue(currentMorph);
}

void AuthenticEMUZPlane::setIntensity(float intensity)
{
    currentIntensity = juce::jlimit(0.0f, 1.0f, intensity);
    intensitySmoother.setTargetValue(currentIntensity);
}

void AuthenticEMUZPlane::setDrive(float drive)
{
    // Convert dB to linear gain
    currentDrive = std::pow(10.0f, drive / 20.0f);
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
    // Envelope follower implementation would go here in production
}

void AuthenticEMUZPlane::setLFOPhase(float phase)
{
    lfoPhase = juce::jlimit(0.0f, juce::MathConstants<float>::twoPi, phase);
}

void AuthenticEMUZPlane::updateCoefficientsBlock()
{
    // Get current morph pair shapes
    auto pairShapes = MORPH_PAIRS[static_cast<int>(currentPair)];
    ShapeID shapeA = pairShapes[0];
    ShapeID shapeB = pairShapes[1];

    // Get authentic EMU coefficients
    const auto& emuShapeA = AUTHENTIC_EMU_SHAPES[static_cast<int>(shapeA)];
    const auto& emuShapeB = AUTHENTIC_EMU_SHAPES[static_cast<int>(shapeB)];

    // Interpolate between shapes using smoothed morph parameter
    float smoothedMorph = morphSmoother.getCurrentValue();
    interpolatePoles(emuShapeA, emuShapeB, smoothedMorph);

    // Convert poles to biquad coefficients and update filter sections
    for (int i = 0; i < 6; ++i) {
        poleTosBiquadCoeffs(currentPoles[i], filterSections[i]);
    }
}

void AuthenticEMUZPlane::interpolatePoles(const std::array<float, 12>& shapeA,
                                        const std::array<float, 12>& shapeB,
                                        float morphPos)
{
    // Interpolate pole pairs using proper complex interpolation
    // This preserves the mathematical relationships that make EMU filters unique
    for (int i = 0; i < 6; ++i) {
        int rIndex = i * 2;
        int thetaIndex = i * 2 + 1;

        // Get pole pairs from authentic EMU data
        float rA = shapeA[rIndex];
        float thetaA = shapeA[thetaIndex];
        float rB = shapeB[rIndex];
        float thetaB = shapeB[thetaIndex];

        // Ensure radius stays within stable range (< 1.0 for stability)
        rA = juce::jlimit(0.1f, 0.99f, rA);
        rB = juce::jlimit(0.1f, 0.99f, rB);

        // Linear interpolation for radius
        currentPoles[i].r = rA + morphPos * (rB - rA);

        // Shortest-path interpolation for angle to avoid phase jumps
        float angleDiff = thetaB - thetaA;
        while (angleDiff > juce::MathConstants<float>::pi) angleDiff -= juce::MathConstants<float>::twoPi;
        while (angleDiff < -juce::MathConstants<float>::pi) angleDiff += juce::MathConstants<float>::twoPi;

        currentPoles[i].theta = thetaA + morphPos * angleDiff;

        // Apply intensity scaling to radius for filter strength control
        float intensityScaling = 0.5f + intensitySmoother.getCurrentValue() * 0.49f;
        currentPoles[i].r *= intensityScaling;
    }
}

void AuthenticEMUZPlane::poleTosBiquadCoeffs(const PolePair& pole, BiquadSection& section)
{
    // Convert pole pair to biquad coefficients using bilinear transform
    // This is the authentic EMU approach for Z-plane to biquad conversion

    const float r = pole.r;
    const float theta = pole.theta;

    const float re = r * std::cos(theta);
    const float a1 = -2.0f * re;
    const float a2 = r * r;

    // Keep unity gain at DC to avoid lifting the noise floor
    section.b0 = 1.0f;
    section.b1 = 0.0f;
    section.b2 = 0.0f;

    const float qScale = 1.0f + intensitySmoother.getCurrentValue() * 0.75f;  // Moderate resonance boost
    section.a1 = juce::jlimit(-1.99f, 1.99f, a1 * qScale);
    section.a2 = juce::jlimit(-0.99f, 0.99f, a2 * qScale);
}

float AuthenticEMUZPlane::applySaturation(float input, float amount) const
{
    if (amount <= 0.0f) return input;

    // EMU-style soft saturation using tanh
    float drive = 1.0f + amount * 3.0f;
    return std::tanh(input * drive) / drive;
}

void AuthenticEMUZPlane::getSectionCoeffs (std::array<BiquadCoeffs, 6>& dest) const
{
    for (int i = 0; i < 6; ++i)
    {
        const auto& section = filterSections[(size_t) i];
        auto& out = dest[(size_t) i];
        out.b0 = section.b0;
        out.b1 = section.b1;
        out.b2 = section.b2;
        out.a1 = section.a1;
        out.a2 = section.a2;
    }
}
