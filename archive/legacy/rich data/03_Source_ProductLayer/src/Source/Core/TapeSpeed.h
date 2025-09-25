#pragma once
#include <JuceHeader.h>
#include <atomic>

/**
 * RT-Safe Tape Speed Processor
 * Variable-rate resampler for vintage tape machine effects
 * Supports fixed speed ratios and wow/flutter modulation
 */
class TapeSpeed
{
public:
    TapeSpeed();
    ~TapeSpeed() = default;

    void prepareToPlay(double sampleRate, int blockSize);
    void processBlock(juce::AudioBuffer<float>& buffer);
    void reset();

    // RT-Safe parameter setters (atomic)
    void setSpeedRatio(float ratio);  // 1.0 = normal, 1.15 = 15% faster
    void setWowFlutter(float amount);  // 0.0-1.0, wow/flutter intensity
    void setMode(int mode);  // 0 = Fixed, 1 = Dynamic (wow/flutter)

    // Latency reporting
    int getLatencySamples() const { return latencySamples.load(std::memory_order_relaxed); }
    
    // SAFETY: Preparation guard
    bool prepared() const { return isPrepared.load(std::memory_order_acquire); }

private:
    // RT-Safe atomic parameters
    std::atomic<float> speedRatio{1.0f};
    std::atomic<float> wowFlutterAmount{0.0f};
    std::atomic<int> processingMode{0};  // 0 = Fixed, 1 = Dynamic
    std::atomic<int> latencySamples{0};
    std::atomic<bool> isPrepared{false}; // SAFETY: Track initialization state

    // Sample rate and buffer management
    double currentSampleRate = 44100.0;
    int maxBlockSize = 512;

    // Simple linear interpolation resampler
    juce::AudioBuffer<float> resampleBuffer;
    float readPosition = 0.0f;
    
    // Wow/flutter LFO
    float wowPhase = 0.0f;
    float flutterPhase = 0.0f;
    const float wowFreq = 0.8f;      // ~0.8 Hz wow
    const float flutterFreq = 6.5f;  // ~6.5 Hz flutter
    
    // Fixed-size delay line for latency compensation
    static constexpr int maxDelaySamples = 2048;
    juce::AudioBuffer<float> delayBuffer;
    int delayWritePos = 0;
    
    // RT-Safe helper methods
    float calculateCurrentRatio() const;
    void processFixedSpeed(juce::AudioBuffer<float>& buffer);
    void processDynamicSpeed(juce::AudioBuffer<float>& buffer);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TapeSpeed)
};