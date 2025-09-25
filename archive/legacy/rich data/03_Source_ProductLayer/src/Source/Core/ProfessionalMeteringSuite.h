/******************************************************************************
 * File: ProfessionalMeteringSuite.h
 * Description: Professional audio metering for SpectralCanvas Pro
 * 
 * Provides broadcast-quality metering including real-time spectrum analysis,
 * LUFS loudness measurement, phase correlation, and performance monitoring.
 * Essential for professional music production workflows.
 * 
 * Copyright (c) 2025 Spectral Audio Systems
 ******************************************************************************/

#pragma once
#include <JuceHeader.h>
#include <memory>
#include <atomic>
#include <array>
#include <vector>
#include <chrono>

//==============================================================================
/**
 * @brief Real-time spectrum analyzer with paint-to-audio integration
 * 
 * Provides high-resolution spectral analysis optimized for visualizing
 * the paint-to-audio synthesis process. Uses windowed FFT with overlapping
 * for smooth, responsive display.
 */
class RealtimeSpectrumAnalyzer
{
public:
    RealtimeSpectrumAnalyzer();
    ~RealtimeSpectrumAnalyzer();
    
    //==============================================================================
    // Configuration
    
    enum class WindowSize
    {
        Size512 = 9,    // 2^9 = 512 samples
        Size1024 = 10,  // 2^10 = 1024 samples  
        Size2048 = 11,  // 2^11 = 2048 samples
        Size4096 = 12   // 2^12 = 4096 samples
    };
    
    enum class WindowType
    {
        Hann,
        Hamming,
        Blackman,
        Kaiser
    };
    
    void prepareToPlay(double sampleRate, int samplesPerBlock);
    void setWindowSize(WindowSize size);
    void setWindowType(WindowType type);
    void setOverlapFactor(float overlap); // 0.5 = 50% overlap
    
    //==============================================================================
    // Processing
    
    void processAudioBlock(const juce::AudioBuffer<float>& buffer);
    void processMonoSample(float sample);
    void processStereoSample(float left, float right);
    
    //==============================================================================
    // Data Access (Thread-safe)
    
    struct SpectrumData
    {
        std::vector<float> magnitudes;      // Linear magnitude values
        std::vector<float> magnitudesDB;    // dB magnitude values
        std::vector<float> phases;          // Phase values (-π to π)
        float peakFrequency = 0.0f;         // Frequency of peak magnitude
        float spectralCentroid = 0.0f;      // Spectral centroid in Hz
        float spectralRolloff = 0.0f;       // 85% rolloff frequency
        std::chrono::high_resolution_clock::time_point timestamp;
        
        SpectrumData() = default;
        SpectrumData(int numBins) : magnitudes(numBins), magnitudesDB(numBins), phases(numBins) {}
    };
    
    SpectrumData getCurrentSpectrum() const;
    std::vector<SpectrumData> getRecentHistory(int numFrames = 10) const;
    
    // Frequency mapping helpers
    float binToFrequency(int bin) const;
    int frequencyToBin(float frequency) const;
    std::vector<float> getFrequencyAxis() const;
    
    //==============================================================================
    // Visual Integration
    
    // Paint-to-audio frequency highlighting
    void highlightFrequencyRange(float minHz, float maxHz, float intensity = 1.0f);
    void clearFrequencyHighlights();
    
    // Get highlighting data for visualization
    std::vector<std::pair<float, float>> getHighlightedRanges() const;
    
    //==============================================================================
    // Performance & Quality Settings
    
    void enableZeroLatencyMode(bool enable);  // Trades quality for latency
    void setUpdateRate(int updatesPerSecond); // 30-120 Hz typical
    void setFrequencyRange(float minHz, float maxHz);
    
    // Statistics
    struct PerformanceStats
    {
        std::atomic<float> processingTimeMs{0.0f};
        std::atomic<int> frameRate{0};
        std::atomic<bool> droppedFrames{false};
        std::atomic<int> bufferUnderruns{0};
    };
    
    const PerformanceStats& getPerformanceStats() const { return performanceStats; }
    
private:
    //==============================================================================
    // FFT Processing
    
    std::unique_ptr<juce::dsp::FFT> fft;
    std::unique_ptr<juce::dsp::WindowingFunction<float>> window;
    
    int fftOrder = 10;  // 1024 samples default
    int fftSize = 1024;
    int bufferSize = 0;
    double sampleRate = 44100.0;
    
    // Processing buffers
    std::vector<float> inputBuffer;
    std::vector<float> windowedBuffer;
    std::vector<float> fftBuffer;      // Real + imaginary interleaved
    std::vector<float> magnitudeBuffer;
    std::vector<float> phaseBuffer;
    
    // Overlap management
    float overlapFactor = 0.75f;       // 75% overlap default
    int hopSize = 256;                 // Samples between analysis frames
    int writeIndex = 0;
    
    //==============================================================================
    // Thread Safety
    
    mutable juce::SpinLock dataLock;
    static constexpr int HISTORY_SIZE = 32;
    std::array<SpectrumData, HISTORY_SIZE> spectrumHistory;
    std::atomic<int> historyWriteIndex{0};
    
    //==============================================================================
    // Frequency Highlighting (for Paint-to-Audio integration)
    
    struct FrequencyHighlight
    {
        float minHz = 0.0f;
        float maxHz = 0.0f;
        float intensity = 1.0f;
        std::chrono::high_resolution_clock::time_point timestamp;
    };
    
    mutable juce::CriticalSection highlightLock;
    std::vector<FrequencyHighlight> activeHighlights;
    
    //==============================================================================
    // Performance Monitoring
    
    PerformanceStats performanceStats;\n    std::chrono::high_resolution_clock::time_point lastUpdateTime;
    int frameCounter = 0;
    
    //==============================================================================
    // Internal Methods
    
    void updateFFTSettings();
    void processFFTFrame();
    void calculateSpectralFeatures(SpectrumData& data);
    void updatePerformanceStats();
    void cleanupOldHighlights();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RealtimeSpectrumAnalyzer)
};

//==============================================================================
/**
 * @brief Professional LUFS loudness meter (EBU R128 compliant)
 * 
 * Implements the ITU-R BS.1770-4 standard for loudness measurement,
 * providing integrated loudness, short-term loudness, and momentary
 * loudness measurements essential for broadcast and mastering.
 */
class LUFSLoudnessMeter
{
public:
    LUFSLoudnessMeter();
    ~LUFSLoudnessMeter();
    
    //==============================================================================
    // Configuration
    
    void prepareToPlay(double sampleRate, int samplesPerBlock, int numChannels);
    void reset();
    
    // Measurement modes
    void enableIntegratedMeasurement(bool enable);   // Long-term integrated
    void enableShortTermMeasurement(bool enable);    // 3-second window
    void enableMomentaryMeasurement(bool enable);    // 400ms window
    
    //==============================================================================
    // Processing
    
    void processAudioBlock(const juce::AudioBuffer<float>& buffer);
    
    //==============================================================================
    // Measurements (Thread-safe)
    
    struct LoudnessMeasurements
    {
        float integratedLUFS = -80.0f;      // Long-term integrated loudness
        float shortTermLUFS = -80.0f;       // Short-term loudness (3s)
        float momentaryLUFS = -80.0f;       // Momentary loudness (400ms)
        
        float truePeakLevel = -80.0f;       // True peak level (dBTP)
        float loudnessRange = 0.0f;         // LRA (loudness range)
        
        bool integratedValid = false;       // True when measurement is stable
        bool shortTermValid = false;
        bool momentaryValid = false;
        
        std::chrono::high_resolution_clock::time_point timestamp;
    };
    
    LoudnessMeasurements getCurrentMeasurements() const;
    
    // Real-time data for metering displays
    std::vector<float> getMomentaryHistory(int numSeconds = 30) const;
    std::vector<float> getShortTermHistory(int numMinutes = 10) const;
    
    //==============================================================================
    // Broadcast Standards Compliance
    
    enum class BroadcastStandard
    {
        EBU_R128,       // -23 LUFS (European)
        ATSC_A85,       // -24 LUFS (US TV)
        ARIB_TR_B32,    // -24 LUFS (Japanese TV)
        Spotify,        // -14 LUFS (streaming)
        YouTube,        // -14 LUFS (streaming)
        Custom
    };
    
    void setBroadcastStandard(BroadcastStandard standard);
    void setCustomTarget(float targetLUFS, float maxTruePeak = -1.0f);
    
    // Compliance checking
    bool isCompliantWithStandard() const;
    float getTargetLoudness() const { return targetLoudness.load(); }
    float getMaxTruePeak() const { return maxTruePeak.load(); }
    
private:
    //==============================================================================
    // ITU-R BS.1770-4 Implementation
    
    // K-weighting pre-filter (high-shelf + high-pass)
    class KWeightingFilter
    {
    public:
        void prepareToPlay(double sampleRate);
        float processSample(float input);
        void reset();
        
    private:\n        // High-shelf filter: +3.99 dB above 1681 Hz
        float highShelf_b0, highShelf_b1, highShelf_b2;
        float highShelf_a1, highShelf_a2;
        float highShelf_x1 = 0.0f, highShelf_x2 = 0.0f;
        float highShelf_y1 = 0.0f, highShelf_y2 = 0.0f;
        
        // High-pass filter: -3 dB at 38 Hz
        float highPass_b0, highPass_b1, highPass_b2;
        float highPass_a1, highPass_a2;
        float highPass_x1 = 0.0f, highPass_x2 = 0.0f;
        float highPass_y1 = 0.0f, highPass_y2 = 0.0f;
    };
    
    std::vector<KWeightingFilter> kWeightingFilters;  // One per channel
    
    // Channel weighting factors (ITU-R BS.1770-4)
    std::vector<float> channelWeights;
    
    //==============================================================================
    // Gating and Integration
    
    class GatedIntegrator
    {
    public:
        void initialize(double sampleRate, float blockDuration);
        void addBlock(float loudness);
        float getIntegratedLoudness() const;
        float getLoudnessRange() const;
        void reset();
        
    private:
        struct LoudnessBlock
        {
            float loudness;
            std::chrono::high_resolution_clock::time_point timestamp;
        };
        
        std::vector<LoudnessBlock> loudnessBlocks;
        float blockDuration = 0.4f;  // 400ms blocks
        mutable juce::CriticalSection blocksLock;
        
        float calculateGatedLoudness(float relativeGate) const;
    };
    
    GatedIntegrator integratedGating;
    
    //==============================================================================
    // Windowed Measurements
    
    template<int WindowSizeMs>
    class WindowedLoudnessMeter
    {
    public:
        void initialize(double sampleRate);
        void addSample(float loudness);
        float getCurrentLoudness() const;
        bool isValid() const { return sampleCount >= windowSize; }
        void reset();
        
    private:\n        static constexpr int windowSize = WindowSizeMs * 44100 / 1000;  // Convert ms to samples at 44.1kHz
        std::vector<float> circularBuffer;
        int writeIndex = 0;
        int sampleCount = 0;
        float sum = 0.0f;
    };
    
    WindowedLoudnessMeter<400> momentaryMeter;   // 400ms window
    WindowedLoudnessMeter<3000> shortTermMeter;  // 3000ms window
    
    //==============================================================================
    // True Peak Detection
    
    class TruePeakDetector
    {
    public:
        void prepareToPlay(double sampleRate, int numChannels);
        void processBlock(const juce::AudioBuffer<float>& buffer);
        float getTruePeakLevel() const { return truePeakLevel.load(); }
        void reset();
        
    private:
        static constexpr int OVERSAMPLING_FACTOR = 4;
        std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
        std::atomic<float> truePeakLevel{-80.0f};
        
        juce::AudioBuffer<float> oversampledBuffer;
    };
    
    TruePeakDetector truePeakDetector;
    
    //==============================================================================
    // State and Configuration
    
    double sampleRate = 44100.0;
    int numChannels = 2;
    int samplesPerBlock = 512;
    
    std::atomic<bool> integratedEnabled{true};
    std::atomic<bool> shortTermEnabled{true};
    std::atomic<bool> momentaryEnabled{true};
    
    // Broadcast standard compliance
    std::atomic<float> targetLoudness{-23.0f};  // LUFS
    std::atomic<float> maxTruePeak{-1.0f};      // dBTP
    
    // Current measurements (thread-safe)
    mutable juce::SpinLock measurementsLock;
    LoudnessMeasurements currentMeasurements;
    
    // History storage for displays
    static constexpr int MOMENTARY_HISTORY_SIZE = 30 * 60;  // 30 seconds at ~60 FPS
    static constexpr int SHORT_TERM_HISTORY_SIZE = 10 * 60; // 10 minutes at ~1 FPS
    
    std::array<float, MOMENTARY_HISTORY_SIZE> momentaryHistory;
    std::array<float, SHORT_TERM_HISTORY_SIZE> shortTermHistory;
    std::atomic<int> momentaryHistoryIndex{0};
    std::atomic<int> shortTermHistoryIndex{0};
    
    //==============================================================================
    // Processing State
    
    float currentBlockLoudness = -80.0f;
    std::chrono::high_resolution_clock::time_point lastIntegratedUpdate;
    
    //==============================================================================
    // Internal Methods
    
    float calculateBlockLoudness(const juce::AudioBuffer<float>& buffer);
    void updateMeasurements();
    void updateBroadcastCompliance();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LUFSLoudnessMeter)
};

//==============================================================================
/**
 * @brief Professional metering suite coordinator
 * 
 * Coordinates all metering components and provides a unified interface
 * for SpectralCanvas Pro's professional audio monitoring needs.
 */
class ProfessionalMeteringSuite
{
public:
    ProfessionalMeteringSuite();
    ~ProfessionalMeteringSuite();
    
    //==============================================================================
    // Lifecycle
    
    void prepareToPlay(double sampleRate, int samplesPerBlock, int numChannels);
    void processAudioBlock(const juce::AudioBuffer<float>& buffer);
    void releaseResources();
    
    //==============================================================================
    // Component Access
    
    RealtimeSpectrumAnalyzer& getSpectrumAnalyzer() { return *spectrumAnalyzer; }
    LUFSLoudnessMeter& getLoudnessMeter() { return *loudnessMeter; }
    
    const RealtimeSpectrumAnalyzer& getSpectrumAnalyzer() const { return *spectrumAnalyzer; }
    const LUFSLoudnessMeter& getLoudnessMeter() const { return *loudnessMeter; }
    
    //==============================================================================
    // Performance Monitoring
    
    struct SystemPerformanceMetrics
    {
        float cpuUsagePercent = 0.0f;
        float memoryUsageMB = 0.0f;
        int audioDropouts = 0;
        float audioLatencyMs = 0.0f;
        
        // Metering-specific performance
        float spectrumAnalyzerCPU = 0.0f;
        float loudnessMeterCPU = 0.0f;
        
        std::chrono::high_resolution_clock::time_point timestamp;
    };
    
    SystemPerformanceMetrics getSystemMetrics() const;
    
    //==============================================================================
    // Master Controls
    
    void enableAllMetering(bool enable);
    void enableSpectrumAnalyzer(bool enable);
    void enableLoudnessMeter(bool enable);
    
    bool isSpectrumAnalyzerEnabled() const { return spectrumEnabled.load(); }
    bool isLoudnessMeterEnabled() const { return loudnessEnabled.load(); }
    
    //==============================================================================
    // Paint-to-Audio Integration
    
    // Highlight frequency ranges being painted
    void notifyPaintFrequencyRange(float minHz, float maxHz, float intensity);
    void notifyPaintStopped();
    
    // Get insights for paint-to-audio correlation
    float getEnergyInFrequencyRange(float minHz, float maxHz) const;
    std::vector<float> getFrequencySpectrum() const;
    
private:
    std::unique_ptr<RealtimeSpectrumAnalyzer> spectrumAnalyzer;
    std::unique_ptr<LUFSLoudnessMeter> loudnessMeter;
    
    // Enable states
    std::atomic<bool> spectrumEnabled{true};
    std::atomic<bool> loudnessEnabled{true};
    
    // Performance monitoring
    mutable SystemPerformanceMetrics performanceMetrics;
    mutable juce::SpinLock metricsLock;
    
    std::chrono::high_resolution_clock::time_point lastPerformanceUpdate;
    
    // Internal methods
    void updatePerformanceMetrics();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProfessionalMeteringSuite)
};