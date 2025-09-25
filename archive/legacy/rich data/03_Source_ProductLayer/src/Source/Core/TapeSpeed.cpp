#include "TapeSpeed.h"
#include "Util/Determinism.h"
TapeSpeed::TapeSpeed()
{
    // Initialize with safe defaults
    speedRatio.store(1.0f, std::memory_order_relaxed);
    wowFlutterAmount.store(0.0f, std::memory_order_relaxed);
    processingMode.store(0, std::memory_order_relaxed);
    latencySamples.store(0, std::memory_order_relaxed);
}

void TapeSpeed::prepareToPlay(double sampleRate, int blockSize)
{
    currentSampleRate = sampleRate;
    maxBlockSize = blockSize;
    
    // Allocate buffers for worst-case resampling (2x speed = 2x samples needed)
    const int maxResampleSize = blockSize * 2;
    resampleBuffer.setSize(2, maxResampleSize);
    delayBuffer.setSize(2, maxDelaySamples);
    
    reset();
    
    // SAFETY: Mark engine as properly initialized
    isPrepared.store(true, std::memory_order_release);
}

void TapeSpeed::reset()
{
    resampleBuffer.clear();
    delayBuffer.clear();
    readPosition = 0.0f;
    wowPhase = 0.0f;
    flutterPhase = 0.0f;
    delayWritePos = 0;
    
    // Calculate fixed latency compensation
    // Simple linear interpolation adds ~1 sample latency
    latencySamples.store(1, std::memory_order_release);
}

void TapeSpeed::processBlock(juce::AudioBuffer<float>& buffer)
{
    if (!prepared()) return;  // SAFETY: Guard against pre-init calls
    
    const int mode = processingMode.load(std::memory_order_relaxed);
    
    if (mode == 0) {
        processFixedSpeed(buffer);
    } else {
        processDynamicSpeed(buffer);
    }
}

void TapeSpeed::processFixedSpeed(juce::AudioBuffer<float>& buffer)
{
    const float ratio = speedRatio.load(std::memory_order_relaxed);
    
    // If ratio is 1.0, bypass processing
    if (std::abs(ratio - 1.0f) < 0.001f) {
        return;  // No processing needed
    }
    
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    
    // Ensure resample buffer is large enough
    const int outputSamples = static_cast<int>(numSamples / ratio) + 2;
    resampleBuffer.setSize(numChannels, outputSamples, false, false, true);
    
    for (int channel = 0; channel < numChannels; ++channel)
    {
        const float* input = buffer.getReadPointer(channel);
        float* output = resampleBuffer.getWritePointer(channel);
        
        // Simple linear interpolation resampling
        float pos = 0.0f;
        for (int i = 0; i < outputSamples && pos < numSamples - 1; ++i)
        {
            const int index = static_cast<int>(pos);
            const float frac = pos - index;
            
            if (index + 1 < numSamples) {
                output[i] = input[index] * (1.0f - frac) + input[index + 1] * frac;
            } else {
                output[i] = input[index];
            }
            
            pos += ratio;
        }
    }
    
    // Copy resampled data back to original buffer
    const int samplesUsed = juce::jmin(numSamples, outputSamples);
    for (int channel = 0; channel < numChannels; ++channel)
    {
        buffer.copyFrom(channel, 0, resampleBuffer, channel, 0, samplesUsed);
        
        // Clear remaining samples if any
        if (samplesUsed < numSamples) {
            buffer.clear(channel, samplesUsed, numSamples - samplesUsed);
        }
    }
}

void TapeSpeed::processDynamicSpeed(juce::AudioBuffer<float>& buffer)
{
    const float baseRatio = speedRatio.load(std::memory_order_relaxed);
    float wowFlutter = wowFlutterAmount.load(std::memory_order_relaxed);
    // Deterministic mode: freeze time-varying drift for reproducibility
    if (SpectralCanvas::Determinism::IsEnabled()) {
        wowFlutter = 0.0f;
    }
    
    if (wowFlutter < 0.001f) {
        // No wow/flutter, use fixed speed
        processFixedSpeed(buffer);
        return;
    }
    
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    const float deltaTime = 1.0f / static_cast<float>(currentSampleRate);
    
    for (int channel = 0; channel < numChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer(channel);
        float localWowPhase = wowPhase;
        float localFlutterPhase = flutterPhase;
        
        for (int sample = 0; sample < numSamples; ++sample)
        {
            // Calculate wow and flutter modulation
            const float wow = std::sin(localWowPhase) * 0.02f;      // ±2% wow
            const float flutter = std::sin(localFlutterPhase) * 0.01f; // ±1% flutter
            
            const float modulation = (wow + flutter) * wowFlutter;
            const float currentRatio = baseRatio * (1.0f + modulation);
            
            // Apply modulation (simplified - could use delay line for better quality)
            channelData[sample] *= (1.0f + modulation * 0.1f);  // Amplitude modulation
            
            // Update phases
            localWowPhase += 2.0f * juce::MathConstants<float>::pi * wowFreq * deltaTime;
            localFlutterPhase += 2.0f * juce::MathConstants<float>::pi * flutterFreq * deltaTime;
            
            // Wrap phases
            if (localWowPhase > 2.0f * juce::MathConstants<float>::pi)
                localWowPhase -= 2.0f * juce::MathConstants<float>::pi;
            if (localFlutterPhase > 2.0f * juce::MathConstants<float>::pi)
                localFlutterPhase -= 2.0f * juce::MathConstants<float>::pi;
        }
    }
    
    // Update global phases
    wowPhase += 2.0f * juce::MathConstants<float>::pi * wowFreq * deltaTime * numSamples;
    flutterPhase += 2.0f * juce::MathConstants<float>::pi * flutterFreq * deltaTime * numSamples;
    
    // Wrap phases
    while (wowPhase > 2.0f * juce::MathConstants<float>::pi)
        wowPhase -= 2.0f * juce::MathConstants<float>::pi;
    while (flutterPhase > 2.0f * juce::MathConstants<float>::pi)
        flutterPhase -= 2.0f * juce::MathConstants<float>::pi;
}

float TapeSpeed::calculateCurrentRatio() const
{
    return speedRatio.load(std::memory_order_relaxed);
}

void TapeSpeed::setSpeedRatio(float ratio)
{
    // Clamp to reasonable range
    const float clampedRatio = juce::jlimit(0.5f, 2.0f, ratio);
    speedRatio.store(clampedRatio, std::memory_order_relaxed);
}

void TapeSpeed::setWowFlutter(float amount)
{
    const float clampedAmount = juce::jlimit(0.0f, 1.0f, amount);
    wowFlutterAmount.store(clampedAmount, std::memory_order_relaxed);
}

void TapeSpeed::setMode(int mode)
{
    const int clampedMode = juce::jlimit(0, 1, mode);
    processingMode.store(clampedMode, std::memory_order_relaxed);
}
