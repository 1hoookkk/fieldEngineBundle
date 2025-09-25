/******************************************************************************
 * File: CDPSpectralEngine.h
 * Description: CDP-style spectral processing engine for real-time paint control
 * 
 * Revolutionary spectral processing system inspired by Composer's Desktop Project
 * with real-time paint-to-spectral parameter mapping. Combines the legendary
 * power of CDP's spectral tools with modern real-time interaction.
 * 
 * Core Features:
 * - Hybrid Phase-Vocoder + FFT processing for optimal quality/performance
 * - CDP-style spectral effects: blur, randomize, shuffle, freeze, arpeggiate
 * - Real-time parameter smoothing for artifact-free paint control
 * - Separate processing thread with lock-free communication
 * - Adaptive windowing based on effect type and performance requirements
 * 
 * Copyright (c) 2025 Spectral Audio Systems
 ******************************************************************************/

#pragma once
#include <JuceHeader.h>
#include <memory>
#include <atomic>
#include <vector>
#include <array>
#include <complex>
#include <thread>
#include <chrono>

// Forward declarations
class PerformanceProfiler;

/**
 * @brief Revolutionary CDP-style spectral processing engine for real-time use
 * 
 * Transforms SpectralCanvas Pro into the world's first real-time CDP-style
 * spectral painting instrument, enabling impossible-to-achieve textures and
 * evolving soundscapes controlled by paint strokes.
 */
class CDPSpectralEngine
{
public:
    CDPSpectralEngine();
    ~CDPSpectralEngine();
    
    //==============================================================================
    // Audio Processing Lifecycle
    
    void prepareToPlay(double sampleRate, int samplesPerBlock, int numChannels);
    void processBlock(juce::AudioBuffer<float>& buffer);
    void releaseResources();
    
    //==============================================================================
    // CDP-Style Spectral Effects
    
    enum class SpectralEffect
    {
        None = 0,
        Blur,           // Frequency domain smoothing
        Randomize,      // Phase/amplitude randomization
        Shuffle,        // Frequency bin reordering
        Freeze,         // Sustain spectral bands
        Arpeggiate,     // Temporal sequencing of frequency bands
        TimeExpand,     // Phase-vocoder time stretching
        Average,        // Spectral averaging across time
        Morph          // Spectral morphing between sources
    };
    
    // Individual effect control
    void setSpectralEffect(SpectralEffect effect, float intensity = 1.0f);
    void setEffectParameter(SpectralEffect effect, int paramIndex, float value);
    float getEffectParameter(SpectralEffect effect, int paramIndex) const;
    
    // Multi-effect layering (for simultaneous color painting)
    void addSpectralLayer(SpectralEffect effect, float intensity, float mix = 1.0f);
    void clearSpectralLayers();
    int getActiveLayerCount() const;
    
    // Visual feedback support for MetaSynth-style canvas
    SpectralEffect getCurrentEffect() const { return currentEffect.load(); }
    float getCurrentIntensity() const { return currentIntensity.load(); }
    
    //==============================================================================
    // Paint-to-Spectral Parameter Mapping
    
    struct PaintSpectralData
    {
        float hue;          // 0.0-1.0, controls effect type
        float saturation;   // 0.0-1.0, controls effect intensity
        float brightness;   // 0.0-1.0, controls amplitude/gain
        float pressure;     // 0.0-1.0, controls blend amount
        float velocity;     // 0.0-1.0, controls parameter modulation speed
        
        // Spatial context (from SpatialSampleGrid)
        float positionX;    // 0.0-1.0, normalized X position
        float positionY;    // 0.0-1.0, normalized Y position
    };
    
    void processPaintSpectralData(const PaintSpectralData& paintData);
    void updateSpectralParameters(const PaintSpectralData& paintData);
    
    //==============================================================================
    // Adaptive Processing Configuration
    
    enum class ProcessingMode
    {
        RealTime,       // Optimized for low latency (smaller FFT, less overlap)
        Quality,        // Optimized for audio quality (larger FFT, more overlap)
        Adaptive        // Automatically adjusts based on current load
    };
    
    void setProcessingMode(ProcessingMode mode);
    ProcessingMode getProcessingMode() const { return currentProcessingMode.load(); }
    
    // Advanced FFT configuration
    void setFFTSize(int fftSize);      // 512, 1024, 2048, 4096
    void setOverlapFactor(float overlap); // 0.5-0.875 (50%-87.5%)
    void setWindowType(juce::dsp::WindowingFunction<float>::WindowingMethod windowType);
    
    int getCurrentFFTSize() const { return currentFFTSize.load(); }
    float getCurrentOverlapFactor() const { return currentOverlapFactor.load(); }
    
    //==============================================================================
    // Performance & Threading
    
    struct ProcessingStats
    {
        std::atomic<float> cpuUsage{0.0f};           // Current CPU usage (0-1)
        std::atomic<int> activeEffects{0};           // Number of active effects
        std::atomic<float> latencyMs{0.0f};          // Current processing latency
        std::atomic<int> bufferUnderruns{0};         // Buffer underrun count
        std::atomic<bool> isProcessingThreadActive{false};
        
        // Spectral analysis statistics
        std::atomic<float> spectralComplexity{0.0f}; // 0-1, complexity of current spectrum
        std::atomic<int> frozenBands{0};             // Number of frozen frequency bands
        std::atomic<float> morphAmount{0.0f};        // Current morph blend amount
    };
    
    const ProcessingStats& getProcessingStats() const { return processingStats; }
    void resetProcessingStats();
    
    // Performance optimization controls
    void setMaxConcurrentEffects(int maxEffects) { maxConcurrentEffects = maxEffects; }
    void enablePerformanceMode(bool enable);     // Reduces quality for performance
    void setLatencyTarget(float targetMs) { latencyTargetMs.store(targetMs); }
    
    //==============================================================================
    // Tempo Synchronization (for BPM-locked effects)
    
    void setHostTempo(double bpm);
    void setHostTimeSignature(int numerator, int denominator);
    void setHostTransportPlaying(bool isPlaying);
    void setHostPPQPosition(double ppqPosition);
    
    // Tempo-sync effect parameters
    void setArpeggiateRate(float noteValue);     // 1.0 = quarter note, 0.25 = 16th note
    void setFreezeDecayTime(float beats);        // Decay time in beats
    void enableTempoSync(bool enable) { tempoSyncEnabled.store(enable); }
    
    //==============================================================================
    // Spectral Analysis & Visualization Support
    
    struct SpectralFrame
    {
        std::vector<float> magnitudes;      // Magnitude spectrum
        std::vector<float> phases;          // Phase spectrum  
        std::vector<float> processedMags;   // Post-effect magnitudes
        float spectralCentroid = 0.0f;      // Brightness measure
        float spectralSpread = 0.0f;        // Spectral width
        float spectralEntropy = 0.0f;       // Spectral complexity
        std::chrono::high_resolution_clock::time_point timestamp;
    };
    
    SpectralFrame getCurrentSpectralFrame() const;
    std::vector<SpectralFrame> getRecentSpectralHistory(int numFrames = 10) const;
    
    // For real-time visualization
    void enableSpectralVisualization(bool enable) { spectralVisualizationEnabled.store(enable); }
    bool isSpectralVisualizationEnabled() const { return spectralVisualizationEnabled.load(); }
    
    //==============================================================================
    // Preset System Integration
    
    struct SpectralPreset
    {
        juce::String name;
        SpectralEffect primaryEffect;
        std::vector<std::pair<SpectralEffect, float>> layeredEffects;
        std::unordered_map<std::string, float> parameters;
        juce::String description;
        juce::Colour associatedColor;      // Recommended paint color
        
        // Performance characteristics  
        float estimatedCPUUsage = 0.5f;    // 0-1 estimate
        ProcessingMode recommendedMode = ProcessingMode::Adaptive;
    };
    
    void loadSpectralPreset(const SpectralPreset& preset);
    SpectralPreset getCurrentPreset() const;
    void saveCurrentAsPreset(const juce::String& name, const juce::String& description);
    
private:
    //==============================================================================
    // Core Processing Engine
    
    // Hybrid processing approach
    std::unique_ptr<juce::dsp::FFT> forwardFFT;
    std::unique_ptr<juce::dsp::FFT> inverseFFT;
    std::unique_ptr<juce::dsp::WindowingFunction<float>> windowFunction;
    
    // Phase-vocoder for time/pitch manipulation
    struct PhaseVocoder
    {
        std::vector<float> analysisWindow;
        std::vector<float> synthesisWindow;
        std::vector<float> previousPhases;
        std::vector<float> phaseAdvances;
        float timeStretchRatio = 1.0f;
        float pitchShiftRatio = 1.0f;
    };
    
    std::unique_ptr<PhaseVocoder> phaseVocoder;
    
    //==============================================================================
    // Spectral Processing Buffers
    
    // FFT processing buffers
    std::vector<std::complex<float>> fftBuffer;
    std::vector<float> windowedInput;
    std::vector<float> overlapBuffer;
    std::vector<float> outputBuffer;
    
    // Spectral data storage
    std::vector<float> currentMagnitudes;
    std::vector<float> currentPhases;
    std::vector<float> processedMagnitudes;
    std::vector<float> processedPhases;
    
    // Multi-frame storage for temporal effects
    std::vector<std::vector<float>> spectralHistory;    // For averaging, morphing
    std::vector<std::vector<float>> frozenSpectrum;     // For freeze effect
    
    //==============================================================================
    // Real-time Threading System
    
    std::unique_ptr<std::thread> processingThread;
    std::atomic<bool> shouldStopProcessing{false};
    
    // Lock-free communication
    struct ProcessingCommand
    {
        enum Type { SetEffect, SetParameter, SetPaintData, SetTempo } type;
        SpectralEffect effect = SpectralEffect::None;
        int paramIndex = 0;
        float value = 0.0f;
        PaintSpectralData paintData;
        double tempoInfo = 120.0;
    };
    
    static constexpr int COMMAND_QUEUE_SIZE = 256;
    std::array<ProcessingCommand, COMMAND_QUEUE_SIZE> commandQueue;
    std::atomic<int> commandQueueWriteIndex{0};
    std::atomic<int> commandQueueReadIndex{0};
    
    void processingThreadFunction();
    void pushCommand(const ProcessingCommand& command);
    bool popCommand(ProcessingCommand& command);
    
    //==============================================================================
    // Configuration & State
    
    std::atomic<ProcessingMode> currentProcessingMode{ProcessingMode::Adaptive};
    std::atomic<int> currentFFTSize{1024};
    std::atomic<float> currentOverlapFactor{0.75f};
    juce::dsp::WindowingFunction<float>::WindowingMethod currentWindowType;
    
    double currentSampleRate = 44100.0;
    int currentSamplesPerBlock = 512;
    int currentNumChannels = 2;
    
    // Effect state
    std::atomic<SpectralEffect> activeEffect{SpectralEffect::None};
    std::atomic<float> effectIntensity{0.0f};
    std::array<std::atomic<float>, 8> effectParameters; // 8 parameters per effect
    
    // Visual feedback state
    std::atomic<SpectralEffect> currentEffect{SpectralEffect::None};
    std::atomic<float> currentIntensity{0.0f};
    
    // Multi-layer effect system
    struct EffectLayer
    {
        SpectralEffect effect = SpectralEffect::None;
        float intensity = 0.0f;
        float mix = 1.0f;
        bool active = false;
    };
    
    static constexpr int MAX_EFFECT_LAYERS = 8;
    std::array<EffectLayer, MAX_EFFECT_LAYERS> effectLayers;
    std::atomic<int> activeLayerCount{0};
    int maxConcurrentEffects = 4;
    
    //==============================================================================
    // Tempo & Synchronization
    
    std::atomic<double> hostTempo{120.0};
    std::atomic<int> hostTimeSignatureNum{4};  
    std::atomic<int> hostTimeSignatureDen{4};
    std::atomic<bool> hostIsPlaying{false};
    std::atomic<double> hostPPQPosition{0.0};
    std::atomic<bool> tempoSyncEnabled{true};
    
    std::atomic<float> latencyTargetMs{10.0f};
    
    //==============================================================================
    // Performance Monitoring
    
    ProcessingStats processingStats;
    std::chrono::high_resolution_clock::time_point lastProcessTime;
    std::unique_ptr<PerformanceProfiler> performanceProfiler;
    
    //==============================================================================
    // Visualization Support
    
    std::atomic<bool> spectralVisualizationEnabled{false};
    static constexpr int SPECTRAL_HISTORY_SIZE = 32;
    std::array<SpectralFrame, SPECTRAL_HISTORY_SIZE> spectralFrameHistory;
    std::atomic<int> spectralFrameIndex{0};
    
    //==============================================================================
    // Parameter Smoothing System
    
    struct ParameterSmoother
    {
        float currentValue = 0.0f;
        float targetValue = 0.0f;
        float smoothingFactor = 0.1f;    // Higher = faster response
        
        void setTarget(float target) { targetValue = target; }
        float getNext() 
        { 
            currentValue += (targetValue - currentValue) * smoothingFactor;
            return currentValue;
        }
        void setSmoothingTime(float timeMs, double sampleRate)
        {
            smoothingFactor = 1.0f - std::exp(-1.0f / (timeMs * 0.001f * sampleRate));
        }
    };
    
    std::array<ParameterSmoother, 16> parameterSmoothers; // For all effect parameters
    
    //==============================================================================
    // Individual Effect Implementation Methods
    
    void applySpectralBlur(std::vector<float>& magnitudes, float intensity);
    void applySpectralRandomize(std::vector<float>& magnitudes, std::vector<float>& phases, float intensity);
    void applySpectralShuffle(std::vector<float>& magnitudes, std::vector<float>& phases, float intensity);
    void applySpectralFreeze(std::vector<float>& magnitudes, std::vector<float>& phases, float intensity);
    void applySpectralArpeggiate(std::vector<float>& magnitudes, std::vector<float>& phases, float rate, float intensity);
    void applySpectralTimeExpand(std::vector<float>& magnitudes, std::vector<float>& phases, float factor);
    void applySpectralAverage(std::vector<float>& magnitudes, int windowSize);
    void applySpectralMorph(std::vector<float>& magnitudes, const std::vector<float>& targetMags, float amount);
    
    // Core processing methods
    void applyActiveSpectralEffects();
    
    // Utility methods
    void updateAdaptiveProcessing();
    void optimizeForPerformance();
    SpectralEffect hueToSpectralEffect(float hue) const;
    void updateProcessingStats();
    void storeSpectralFrame();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CDPSpectralEngine)
};