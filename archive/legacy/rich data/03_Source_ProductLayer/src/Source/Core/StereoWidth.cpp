#include "StereoWidth.h"

StereoWidth::StereoWidth()
{
    // Initialize with safe default
    widthAmount.store(1.0f, std::memory_order_relaxed);
}

void StereoWidth::prepareToPlay(double sampleRate, int blockSize)
{
    // No preparation needed for simple M/S processing
    reset();
    
    // SAFETY: Mark engine as properly initialized
    isPrepared.store(true, std::memory_order_release);
}

void StereoWidth::reset()
{
    // No state to reset for stateless M/S processing
}

void StereoWidth::processBlock(juce::AudioBuffer<float>& buffer)
{
    if (!prepared()) return;  // SAFETY: Guard against pre-init calls
    
    const float width = widthAmount.load(std::memory_order_relaxed);
    
    // If width is 1.0, bypass processing
    if (std::abs(width - 1.0f) < 0.001f) {
        return;  // No processing needed
    }
    
    // Only process stereo signals
    if (buffer.getNumChannels() < 2) {
        return;  // Mono or no channels
    }
    
    processMidSide(buffer);
}

void StereoWidth::processMidSide(juce::AudioBuffer<float>& buffer)
{
    const float width = widthAmount.load(std::memory_order_relaxed);
    const int numSamples = buffer.getNumSamples();
    
    float* leftChannel = buffer.getWritePointer(0);
    float* rightChannel = buffer.getWritePointer(1);
    
    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float left = leftChannel[sample];
        const float right = rightChannel[sample];
        
        // Convert to Mid/Side
        const float mid = (left + right) * 0.5f;
        const float side = (left - right) * 0.5f;
        
        // Apply width to side signal
        const float wideSide = side * width;
        
        // Convert back to Left/Right
        leftChannel[sample] = mid + wideSide;
        rightChannel[sample] = mid - wideSide;
    }
}

void StereoWidth::setWidth(float width)
{
    // Clamp to reasonable range (0.0 = mono, 1.0 = normal, 2.0 = maximum width)
    const float clampedWidth = juce::jlimit(0.0f, 2.0f, width);
    widthAmount.store(clampedWidth, std::memory_order_relaxed);
}