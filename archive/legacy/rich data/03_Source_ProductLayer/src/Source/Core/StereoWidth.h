#pragma once
#include <JuceHeader.h>
#include <atomic>

/**
 * RT-Safe Stereo Width Processor
 * Simple mid/side width control for vintage machine character
 * Applied at the very end of the processing chain
 */
class StereoWidth
{
public:
    StereoWidth();
    ~StereoWidth() = default;

    void prepareToPlay(double sampleRate, int blockSize);
    void processBlock(juce::AudioBuffer<float>& buffer);
    void reset();

    // RT-Safe parameter setter (atomic)
    void setWidth(float width);  // 1.0 = normal, 1.4 = wide

    // Latency reporting (none for this processor)
    int getLatencySamples() const { return 0; }
    
    // SAFETY: Preparation guard
    bool prepared() const { return isPrepared.load(std::memory_order_acquire); }

private:
    // RT-Safe atomic parameter
    std::atomic<float> widthAmount{1.0f};
    std::atomic<bool> isPrepared{false}; // SAFETY: Track initialization state

    // Simple M/S processing - no delay required
    void processMidSide(juce::AudioBuffer<float>& buffer);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StereoWidth)
};