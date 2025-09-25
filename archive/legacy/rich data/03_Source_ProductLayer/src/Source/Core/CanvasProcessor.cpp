#include "CanvasProcessor.h"
#include <JuceHeader.h>
#include <cmath>


CanvasProcessor::CanvasProcessor()
{
    // Pre-allocate the vector to its maximum size to avoid reallocations
    oscillators.resize(maxPartials);
}

CanvasProcessor::~CanvasProcessor() = default;

void CanvasProcessor::prepareToPlay(double sr, int samplesPerBlock)
{
    sampleRate = static_cast<float>(sr);
    masterGain.reset(sampleRate, 0.01); // 10ms smoothing time
    masterGain.setCurrentAndTargetValue(1.0f);

    for (auto& osc : oscillators)
    {
        osc.phase = 0.0f;
        osc.amplitude = 0.0f;
        osc.targetAmplitude = 0.0f;
    }
}

void CanvasProcessor::processBlock(juce::AudioBuffer<float>& buffer)
{
    juce::ScopedNoDenormals noDenormals;
    if (!isActive || !currentImage.isValid())
    {
        buffer.clear();
        return;
    }

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // Update oscillator properties from the image column once per block
    const int currentColumn = juce::jlimit(0, imageWidth - 1, static_cast<int>(playheadPos * imageWidth));
    updateOscillatorsFromColumn(currentColumn);

    // Get raw pointers to buffer channels for efficient writing
    auto* leftChannel = buffer.getWritePointer(0);
    auto* rightChannel = numChannels > 1 ? buffer.getWritePointer(1) : nullptr;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float leftSample = 0.0f;
        float rightSample = 0.0f;

        // --- SINGLE-PASS RENDER LOOP ---
        for (auto& osc : oscillators)
        {
            // Only process audible oscillators
            if (osc.targetAmplitude > 0.0001f || osc.amplitude > 0.0001f)
            {
                // Smooth amplitude changes to prevent clicks/zippering
                osc.amplitude += (osc.targetAmplitude - osc.amplitude) * 0.05f;

                const float oscSample = osc.getSample() * osc.amplitude;

                if (usePanning && rightChannel != nullptr)
                {
                    leftSample += oscSample * (1.0f - osc.pan);
                    rightSample += oscSample * osc.pan;
                }
                else // Mono or panning is disabled
                {
                    leftSample += oscSample;
                }

                // Advance phase for the next sample
                osc.updatePhase(osc.frequency / sampleRate);
            }
        }

        const float currentGain = masterGain.getNextValue() * amplitudeScale;

        // Write the final computed sample to the output buffer
        leftChannel[sample] = leftSample * currentGain;

        if (rightChannel != nullptr)
        {
            // If panning was disabled, copy the left sample. Otherwise, use the calculated right sample.
            rightChannel[sample] = usePanning ? (rightSample * currentGain) : leftChannel[sample];
        }
    }
}

void CanvasProcessor::updateFromImage(const juce::Image& image)
{
    currentImage = image;
    imageWidth = image.getWidth();
    imageHeight = image.getHeight();

    // Reset all oscillators when a new image is loaded
    for (auto& osc : oscillators)
    {
        osc.amplitude = 0.0f;
        osc.targetAmplitude = 0.0f;
    }
}

void CanvasProcessor::updateOscillatorsFromColumn(int x)
{
    if (!currentImage.isValid() || x < 0 || x >= imageWidth)
        return;

    // Use jmax to prevent division by zero if image is tiny
    const int step = juce::jmax(1, imageHeight / maxPartials);
    int oscIndex = 0;

    const bool isColor = currentImage.getFormat() != juce::Image::PixelFormat::SingleChannel;


    for (int y = 0; y < imageHeight && oscIndex < maxPartials; y += step, ++oscIndex)
    {
        auto pixel = currentImage.getPixelAt(x, y);
        auto& osc = oscillators[oscIndex];

        // Map Y-axis to frequency
        osc.frequency = pixelYToFrequency(y);

        // Map brightness to target amplitude
        osc.targetAmplitude = pixel.getBrightness();

        // Map hue to pan only if the image is in color
        osc.pan = isColor ? pixel.getHue() : 0.5f;
    }
}

float CanvasProcessor::pixelYToFrequency(int y) const
{
    // Logarithmic mapping of pixel Y to frequency for a more musical result
    // Invert Y so that top of image is high frequency
    const float normalisedY = 1.0f - (static_cast<float>(y) / static_cast<float>(imageHeight));

    // Convert linear (0-1) to logarithmic frequency scale
    const float logMin = std::log(minFreq);
    const float logMax = std::log(maxFreq);
    return std::exp(logMin + normalisedY * (logMax - logMin));
}

void CanvasProcessor::setFrequencyRange(float minHz, float maxHz)
{
    minFreq = juce::jlimit(20.0f, 20000.0f, minHz);
    maxFreq = juce::jlimit(minFreq, 22000.0f, maxHz);
}

void CanvasProcessor::setPlayheadPosition(float normalisedPosition)
{
    playheadPos = juce::jlimit(0.0f, 1.0f, normalisedPosition);
}