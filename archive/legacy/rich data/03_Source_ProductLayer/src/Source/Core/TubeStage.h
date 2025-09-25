#pragma once
#include <JuceHeader.h>
#include <array>

/**
 * RT-safe tube saturation with oversampling
 * Cubic soft-clip with bias control and auto-gain compensation
 */
class TubeStage
{
public:
    TubeStage() = default;
    ~TubeStage() = default;
    
    void prepare(double sampleRate, int maxBlockSize);
    void reset();
    void process(juce::AudioBuffer<float>& buffer);
    
    // Parameters
    void setDrive(float db) noexcept { targetDrive = juce::jlimit(0.0f, 24.0f, db); }
    void setBias(float value) noexcept { targetBias = juce::jlimit(-1.0f, 1.0f, value); }
    void setTone(float value) noexcept { targetTone = juce::jlimit(-1.0f, 1.0f, value); }
    void setOversampling(int factor) noexcept { oversampleFactor = juce::jlimit(1, 4, factor); }
    void setOutput(float db) noexcept { targetOutput = juce::jlimit(-12.0f, 12.0f, db); }
    void setAutoGain(bool enable) noexcept { autoGainEnabled = enable; }
    
private:
    // Oversampling buffers (pre-allocated for max 4x)
    juce::AudioBuffer<float> oversampledBuffer;
    juce::AudioBuffer<float> tempBuffer;
    
    // IIR filter coefficients for oversampling (Butterworth)
    struct FilterCoeffs
    {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
        float a1 = 0.0f, a2 = 0.0f;
    };
    
    // Filter states for up/downsampling
    struct FilterState
    {
        float x1 = 0.0f, x2 = 0.0f;
        float y1 = 0.0f, y2 = 0.0f;
    };
    
    // Per-channel filter states
    std::array<FilterState, 2> upsampleStates;
    std::array<FilterState, 2> downsampleStates;
    
    // Pre-calculated filter coefficients for each OS factor
    std::array<FilterCoeffs, 4> upsampleCoeffs;  // 1x, 2x, 3x, 4x
    std::array<FilterCoeffs, 4> downsampleCoeffs;
    
    // Current and target parameters
    float currentDrive = 0.0f;
    float currentBias = 0.0f;
    float currentTone = 0.0f;
    float currentOutput = 0.0f;
    
    float targetDrive = 0.0f;
    float targetBias = 0.0f;
    float targetTone = 0.0f;
    float targetOutput = 0.0f;
    
    // Processing parameters
    double sampleRate = 48000.0;
    int oversampleFactor = 2;
    bool autoGainEnabled = true;
    
    // Smoothing
    float smoothingCoeff = 0.995f;
    
    // Auto-gain compensation LUT (256 entries for drive 0-24dB)
    std::array<float, 256> autoGainLUT;
    
    // Tone control filter state
    std::array<FilterState, 2> toneStates;
    FilterCoeffs toneCoeffs;
    
    // Helper functions
    void calculateFilterCoefficients();
    void updateSmoothing();
    
    // Processing functions
    void upsample(const juce::AudioBuffer<float>& input, juce::AudioBuffer<float>& output);
    void downsample(const juce::AudioBuffer<float>& input, juce::AudioBuffer<float>& output);
    void processSaturation(juce::AudioBuffer<float>& buffer);
    
    // Cubic soft-clip with bias
    inline float cubicClip(float x, float bias) noexcept
    {
        const float x2 = x * x;
        const float x3 = x2 * x;
        
        // Asymmetric cubic: y = x - k*x^3 + bias*x^2
        const float k = 0.333333f;
        float y = x - k * x3 + bias * x2 * 0.5f;
        
        // Hard limit at ±1
        return juce::jlimit(-1.0f, 1.0f, y);
    }
    
    // Fast tanh approximation for additional smoothness
    inline float fastTanh(float x) noexcept
    {
        const float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }
};