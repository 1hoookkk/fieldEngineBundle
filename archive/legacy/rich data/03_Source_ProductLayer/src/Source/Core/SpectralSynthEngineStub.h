#pragma once
#include <JuceHeader.h>
#include "PaintQueue.h"
#include <atomic>
#include <array>
#include <cmath>

/**
 * @brief Simplified RT-safe SpectralSynthEngine stub for Y2K theme implementation
 * 
 * This is a minimal, RT-safe implementation that:
 * - Consumes paint gestures from the PaintQueue
 * - Generates audible spectral synthesis 
 * - Maintains strict RT-safety (no allocations, no locks)
 * - Provides clear audio feedback for paint-to-audio pipeline testing
 * 
 * This stub will be replaced with the full SpectralSynthEngine later,
 * but serves as a foundation for the Y2K theme implementation.
 */
class SpectralSynthEngineStub
{
public:
    SpectralSynthEngineStub();
    ~SpectralSynthEngineStub();

    // Audio processing lifecycle
    void prepareToPlay(double sampleRate, int samplesPerBlock, int numChannels);
    void processBlock(juce::AudioBuffer<float>& buffer, SpectralPaintQueue* paintQueue);
    void releaseResources();

    // Paint-to-audio configuration
    void setFrequencyRange(float minHz, float maxHz) noexcept;
    void setGain(float gain) noexcept;
    void setDecayRate(float decayRate) noexcept;

    // Status queries (for HUD display)
    int getActiveOscillatorCount() const noexcept { return activeOscillatorCount_.load(); }
    float getCurrentCpuLoad() const noexcept { return currentCpuLoad_.load(); }
    
    // Emergency test tone (for debugging audio pipeline)
    void enableTestTone(bool enable) noexcept { testToneEnabled_.store(enable); }
    void setTestToneFrequency(float frequency) noexcept { testToneFrequency_.store(frequency); }

private:
    // Simple spectral oscillator for paint-to-audio synthesis
    struct SimpleOscillator
    {
        std::atomic<bool> active{false};
        std::atomic<float> frequency{440.0f};
        std::atomic<float> amplitude{0.0f};
        std::atomic<float> decayRate{0.95f};
        
        // Audio thread only - no atomics needed
        float phase = 0.0f;
        float currentAmplitude = 0.0f;
        int decayCounter = 0;
        
        void reset() noexcept;
        float renderSample(double sampleRate) noexcept;
        void trigger(float freq, float amp, float decay) noexcept;
    };

    // Configuration
    static constexpr int kMaxOscillators = 64;
    static constexpr int kDecayTime = 44100; // 1 second at 44.1kHz
    
    // Oscillator pool
    std::array<SimpleOscillator, kMaxOscillators> oscillators_;
    std::atomic<int> nextOscillatorIndex_{0};
    std::atomic<int> activeOscillatorCount_{0};
    
    // Audio parameters (RT-safe atomics)
    std::atomic<double> sampleRate_{44100.0};
    std::atomic<float> minFrequencyHz_{80.0f};   // Lower range for more bass
    std::atomic<float> maxFrequencyHz_{8000.0f}; // Upper range for sparkle
    std::atomic<float> masterGain_{0.3f};        // Moderate gain to prevent clipping
    std::atomic<float> defaultDecayRate_{0.996f}; // Smooth decay
    
    // Test tone for debugging
    std::atomic<bool> testToneEnabled_{false};
    std::atomic<float> testToneFrequency_{440.0f};
    float testTonePhase_ = 0.0f;
    
    // Performance monitoring
    std::atomic<float> currentCpuLoad_{0.0f};
    juce::Time lastProcessTime_;
    
    // Internal methods
    SimpleOscillator* findFreeOscillator() noexcept;
    void processPaintGestures(SpectralPaintQueue* paintQueue) noexcept;
    void renderOscillators(juce::AudioBuffer<float>& buffer) noexcept;
    void updatePerformanceMetrics() noexcept;
    
    // Coordinate conversion (optimized with lookup tables later)
    float normalizedYToFrequency(float normalizedY) const noexcept;
    float pressureToAmplitude(float pressure) const noexcept;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectralSynthEngineStub)
};