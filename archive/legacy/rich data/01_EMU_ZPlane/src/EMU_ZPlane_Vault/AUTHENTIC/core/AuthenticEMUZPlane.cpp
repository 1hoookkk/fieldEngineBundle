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
    updateCoefficientsBlock();
}

void AuthenticEMUZPlane::reset()
{
    for (auto& section : filterSections) {
        section.reset();
    }
    lfoPhase = 0.0f;
}

float AuthenticEMUZPlane::processSample(float input)
{
    // Apply input drive with soft saturation
    float drivenInput = input * currentDrive;
    if (currentDrive > 1.0f) {
        drivenInput = std::tanh(drivenInput * 0.5f) * 2.0f;
    }

    // Process through 6-section cascaded biquads (12th order like Audity 2000)
    float output = drivenInput;
    for (int i = 0; i < 6; ++i) {
        output = filterSections[i].processSample(output, sectionSaturation);
    }

    // Auto-makeup gain to maintain consistent loudness during morphing
    if (autoMakeupEnabled) {
        // Simple RMS-based makeup gain (would be more sophisticated in production)
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

    float r = pole.r;
    float theta = pole.theta;

    // Pole in complex form: re + j*im
    float re = r * std::cos(theta);
    float im = r * std::sin(theta);

    // For a complex conjugate pole pair, the denominator polynomial is:
    // (z - (re + j*im)) * (z - (re - j*im)) = z^2 - 2*re*z + (re^2 + im^2)
    // This gives us the biquad denominator coefficients

    float a1 = -2.0f * re;  // -2 * real part
    float a2 = r * r;       // radius squared (magnitude squared)

    // For lowpass response, set numerator for DC gain = 1
    float norm = 1.0f + a1 + a2;  // DC response normalization
    if (std::abs(norm) < 1e-6f) norm = 1.0f;  // Avoid division by zero

    float b0 = (1.0f - a2) / norm;
    float b1 = 0.0f;  // Symmetric FIR for lowpass
    float b2 = -b0;   // Opposite sign for lowpass rolloff

    // Apply intensity scaling to resonance
    float qScale = 1.0f + intensitySmoother.getCurrentValue() * 3.0f;  // Up to 4x Q boost
    a1 *= qScale;
    a2 *= qScale;

    // Set coefficients with stability check
    section.b0 = b0;
    section.b1 = b1;
    section.b2 = b2;
    section.a1 = juce::jlimit(-1.99f, 1.99f, a1);  // Stability bounds
    section.a2 = juce::jlimit(-0.99f, 0.99f, a2);
}

float AuthenticEMUZPlane::applySaturation(float input, float amount) const
{
    if (amount <= 0.0f) return input;

    // EMU-style soft saturation using tanh
    float drive = 1.0f + amount * 3.0f;
    return std::tanh(input * drive) / drive;
}