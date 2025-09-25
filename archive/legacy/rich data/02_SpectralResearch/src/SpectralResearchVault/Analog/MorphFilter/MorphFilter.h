#pragma once
#include <JuceHeader.h>
#include <array>

/**
 * RT-safe morphing filter with curated responses
 * E-mu Audity-inspired multimode filter with smooth morphing
 */
class MorphFilter
{
public:
    MorphFilter() = default;
    ~MorphFilter() = default;
    
    void prepare(double sampleRate, int maxBlockSize);
    void reset();
    void process(juce::AudioBuffer<float>& buffer);
    
    // Parameters (0-1 normalized)
    void setMorph(float value) noexcept { targetMorph = juce::jlimit(0.0f, 1.0f, value); }
    void setCutoff(float hz) noexcept { targetCutoff = juce::jlimit(20.0f, 20000.0f, hz); }
    void setResonance(float q) noexcept { targetResonance = juce::jlimit(0.1f, 10.0f, q); }
    void setDrive(float db) noexcept { targetDrive = juce::jlimit(0.0f, 24.0f, db); }
    void setPrePost(bool pre) noexcept { preMode = pre; }
    
private:
    // Filter topology: State Variable Filter for smooth morphing
    struct SVFState
    {
        float z1 = 0.0f;  // Integrator 1 state
        float z2 = 0.0f;  // Integrator 2 state
    };
    
    // Coefficient structure
    struct Coefficients
    {
        float g = 0.0f;    // Frequency coefficient
        float k = 0.0f;    // Damping coefficient
        float a1 = 0.0f;   // Feedback coefficient
        float a2 = 0.0f;   // Feedforward coefficient
        float a3 = 0.0f;   // Mix coefficient
    };
    
    // Pre-calculated response coefficients (5 curated responses)
    enum ResponseType { LP = 0, BP, HP, NOTCH, VOWEL, NUM_RESPONSES };
    std::array<Coefficients, NUM_RESPONSES> responses;
    
    // Per-channel state
    std::array<SVFState, 2> channelStates;
    
    // Current and target parameters
    float currentMorph = 0.0f;
    float currentCutoff = 1000.0f;
    float currentResonance = 1.0f;
    float currentDrive = 0.0f;
    
    float targetMorph = 0.0f;
    float targetCutoff = 1000.0f;
    float targetResonance = 1.0f;
    float targetDrive = 0.0f;
    
    // Processing state
    double sampleRate = 44100.0;
    bool preMode = true;  // Pre/post drive
    
    // Parameter smoothing
    static constexpr float smoothingFactor = 0.02f;
    
    void updateCoefficients();
    float processSample(float input, int channel);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MorphFilter)
};