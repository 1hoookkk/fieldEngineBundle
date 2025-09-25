/**
 * EMU Audity-Style Filter
 * Mathematical modeling of the legendary SSM2040-inspired multimode filter
 * Captures the warm, musical character of classic EMU filters
 */

#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <atomic>
#include <cmath>

/**
 * EMU Filter Core
 * Models the SSM2040-style 4-pole multimode filter with EMU characteristics
 */
class EMUFilterCore
{
public:
    EMUFilterCore();
    ~EMUFilterCore() = default;
    
    // Filter modes (like classic EMU romplers)
    enum FilterType
    {
        LowPass = 0,     // 24dB/octave low-pass
        HighPass = 1,    // 24dB/octave high-pass  
        BandPass = 2,    // 12dB/octave band-pass
        Notch = 3,       // Notch/band-reject
        AllPass = 4      // All-pass (phase only)
    };
    
    // Audio processing
    void prepareToPlay(double sampleRate);
    float processSample(float input);
    void processBlock(float* samples, int numSamples);
    void reset();
    
    // Filter parameters
    void setCutoffFrequency(float frequency);    // 20Hz - 20kHz
    void setResonance(float resonance);          // 0.0 - 1.0 (0.0 - 0.99 internally)
    void setFilterType(FilterType type);
    void setDrive(float drive);                  // 0.0 - 2.0 (EMU-style saturation)
    void setKeyTracking(float amount);           // 0.0 - 1.0
    
    // Modulation inputs
    void modulateCutoff(float modAmount);        // ±2 octaves
    void modulateResonance(float modAmount);     // ±0.5 resonance
    
    // EMU character controls
    void setVintageMode(bool enabled);           // Adds analog drift and nonlinearity
    void setFilterAge(float age);                // 0.0-1.0, simulates component aging
    void setTemperatureDrift(float temp);        // ±0.1, simulates temperature effects
    
    // Real-time parameter access (for UI visualization)
    float getCurrentCutoff() const { return currentCutoff; }
    float getCurrentResonance() const { return currentResonance; }
    FilterType getCurrentType() const { return currentType; }
    
    // Frequency response (for visual display)
    struct FrequencyResponse
    {
        std::array<float, 512> magnitude;       // Magnitude response
        std::array<float, 512> phase;           // Phase response
        std::array<float, 512> frequencies;     // Frequency points (Hz)
    };
    FrequencyResponse calculateFrequencyResponse() const;
    
private:
    // Filter state variables (Chamberlin state-variable topology)
    float delay1 = 0.0f, delay2 = 0.0f, delay3 = 0.0f, delay4 = 0.0f;
    float feedback = 0.0f;
    
    // Filter coefficients
    float f = 0.0f;      // Frequency coefficient
    float q = 0.0f;      // Resonance coefficient
    float drive = 1.0f;  // Drive amount
    
    // Current parameters
    float currentCutoff = 1000.0f;
    float currentResonance = 0.0f;
    FilterType currentType = LowPass;
    float currentDrive = 1.0f;
    float keyTrackAmount = 0.0f;
    
    // Modulation
    float cutoffModulation = 0.0f;
    float resonanceModulation = 0.0f;
    
    // EMU character simulation
    bool vintageMode = true;
    float filterAge = 0.1f;
    float temperatureDrift = 0.0f;
    juce::Random vintageRandom;
    
    // Audio processing state
    double sampleRate = 44100.0;
    float nyquistFreq = 22050.0f;
    
    // Internal methods
    void updateCoefficients();
    float applySaturation(float input) const;
    float applyVintageCharacter(float input);
    float frequencyToCoeff(float frequency) const;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EMUFilterCore)
};

/**
 * EMU Dual Filter
 * Two filters in series/parallel like some EMU models
 */
class EMUDualFilter
{
public:
    EMUDualFilter();
    ~EMUDualFilter() = default;
    
    enum RoutingMode
    {
        Series = 0,      // Filter1 → Filter2
        Parallel = 1,    // Filter1 + Filter2
        StereoSplit = 2  // Filter1=Left, Filter2=Right
    };
    
    // Audio processing
    void prepareToPlay(double sampleRate);
    void processBlock(juce::AudioSampleBuffer& buffer);
    void reset();
    
    // Dual filter configuration
    void setRoutingMode(RoutingMode mode);
    void setFilterBalance(float balance);        // 0.0=Filter1, 1.0=Filter2
    void linkFilters(bool linked);               // Link parameters for parallel control
    
    // Filter access
    EMUFilterCore& getFilter1() { return filter1; }
    EMUFilterCore& getFilter2() { return filter2; }
    
private:
    EMUFilterCore filter1, filter2;
    RoutingMode routingMode = Series;
    float filterBalance = 0.5f;
    bool filtersLinked = true;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EMUDualFilter)
};

/**
 * Complete EMU Filter System
 * Integrates filter with envelopes, LFOs, and paint canvas control
 */
class EMUFilter
{
public:
    EMUFilter();
    ~EMUFilter() = default;
    
    // Audio processing interface
    void prepareToPlay(double sampleRate, int samplesPerBlock);
    void processBlock(juce::AudioSampleBuffer& buffer);
    void releaseResources();
    
    // Main filter controls (thread-safe for UI)
    void setCutoff(float cutoff);                // 0.0-1.0 normalized
    void setResonance(float resonance);          // 0.0-1.0 normalized
    void setFilterType(int type);                // 0-4 (FilterType enum)
    void setDrive(float drive);                  // 0.0-2.0
    void setKeyTracking(float amount);           // 0.0-1.0
    
    // Envelope and modulation
    void setEnvelopeAmount(float amount);        // ±2 octaves
    void setLFOAmount(float amount);             // ±1 octave  
    void setVelocityAmount(float amount);        // 0.0-1.0
    
    // Paint canvas integration
    void handlePaintStroke(float x, float y, float pressure, juce::Colour color);
    void setPaintMapping(int xMap, int yMap, int pressureMap);
    
    // Advanced features (premium)
    void enableDualFilter(bool enabled);
    void setFilterRouting(int routing);          // 0=Series, 1=Parallel, 2=Stereo
    void setVintageMode(bool enabled);
    void setFilterAge(float age);
    
    // Real-time feedback (for UI visualization)
    struct FilterStatus
    {
        float currentCutoff;
        float currentResonance;
        int currentType;
        float cpuUsage;
        bool isProcessing;
    };
    FilterStatus getFilterStatus() const;
    
    // Frequency response for spectral overlay
    void getFrequencyResponse(float* magnitudes, float* frequencies, int numPoints) const;
    
private:
    // Core filter (always available)
    EMUDualFilter dualFilter;
    
    // Thread-safe parameter communication
    std::atomic<float> atomicCutoff{0.5f};
    std::atomic<float> atomicResonance{0.0f};
    std::atomic<int> atomicFilterType{0};
    std::atomic<float> atomicDrive{1.0f};
    std::atomic<bool> dualFilterEnabled{false};
    
    // Modulation amounts
    float envelopeAmount = 0.0f;
    float lfoAmount = 0.0f;
    float velocityAmount = 0.0f;
    float keyTrackAmount = 0.0f;
    
    // Paint mapping
    int xAxisMapping = 0;     // 0=Cutoff, 1=Resonance, 2=Drive
    int yAxisMapping = 0;     // 0=Cutoff, 1=Type, 2=Resonance
    int pressureMapping = 0;  // 0=Drive, 1=Resonance, 2=Cutoff
    
    // Performance monitoring
    mutable std::atomic<float> cpuUsage{0.0f};
    
    // Internal methods
    void updateParameters();
    float mapPaintToCutoff(float value) const;
    float mapPaintToResonance(float value) const;
    float mapPaintToDrive(float value) const;
    int mapPaintToType(float value) const;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EMUFilter)
};