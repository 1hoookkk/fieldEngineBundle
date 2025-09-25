#pragma once

#include <cstdint>
#include <array>
#include <atomic>
#include <memory>
#include <chrono>
#include "RealtimeSafeTypes.h"
#include "ParameterSnapshot.h"
#include "ReconfigManager.h"

/**
 * SpectralCoreEngine - Pure DSP engine with no JUCE dependencies
 * 
 * This implements Pattern #1: Thin Plugin Wrapper, Fat Engine
 * The engine is completely headless and uses only POD types for communication.
 * Plugin wrapper classes handle JUCE integration and marshal data to/from engine.
 */
class SpectralCoreEngine
{
public:
    // Engine state snapshot for telemetry (POD)
    struct EngineStateSnapshot
    {
        uint64_t frameId;                     // Current engine frame
        uint64_t processedSamples;            // Total samples processed
        float cpuLoadPercent;                 // Current CPU usage
        int activeFFTSize;                    // Current FFT size
        int activeBinCount;                   // Bins above threshold
        float peakMagnitude;                  // Current spectral peak
        float spectralCentroid;               // Brightness measure
        uint32_t maskColumnsProcessed;        // Paint events applied
        uint32_t maskColumnsDropped;          // Paint events dropped
        bool rtViolation;                     // RT-safety violation detected
    };
    
    // Commands for engine reconfiguration (POD)
    struct EngineCommand
    {
        enum Type : uint8_t
        {
            SetFFTSize,
            SetHopSize,
            LoadSample,
            SetSynthMode,
            ApplyMask,
            ResetState
        };
        
        Type type;
        uint32_t version = 1;
        uint64_t engineFrameId = 0;          // When to apply (0 = immediately)
        
        union Data
        {
            struct { uint16_t size; } fftSize;
            struct { uint16_t size; } hopSize;
            struct { uint8_t mode; } synthMode;
            struct { float* maskData; uint32_t numBins; } mask;
        } data;
        
        EngineCommand() : type(ResetState) { data = {}; }
    };
    
    SpectralCoreEngine() noexcept;
    ~SpectralCoreEngine() noexcept;
    
    // Non-copyable for RT safety
    SpectralCoreEngine(const SpectralCoreEngine&) = delete;
    SpectralCoreEngine& operator=(const SpectralCoreEngine&) = delete;
    
    // Initialization (may allocate - call from non-RT thread)
    bool prepare(const ReconfigManager::EngineConfig& config);
    void reset() noexcept;
    
    // Main processing entry point (RT-safe, noexcept)
    void process(const float* const* input, float* const* output, 
                int numChannels, int numSamples, 
                const ParameterSnapshot& params) noexcept;
    
    // Command processing (RT-safe)
    void applyCommand(const EngineCommand& cmd) noexcept;
    
    // Spectral data extraction (RT-safe)
    bool extractSpectralData(float* magnitudes, float* phases, int maxBins) noexcept;
    
    // State queries (RT-safe)
    EngineStateSnapshot getStateSnapshot() const noexcept;
    bool isInitialized() const noexcept { return initialized_.load(std::memory_order_acquire); }
    uint64_t getCurrentFrameId() const noexcept { return currentFrameId_.load(std::memory_order_relaxed); }
    
    // Mask application (RT-safe - called per paint event)
    void applyMaskColumn(const float* maskValues, uint32_t numBins, 
                        uint64_t frameId) noexcept;
    
    // Sample loading interface (called from UI thread)
    bool loadSampleData(const float* sampleData, int numSamples, 
                       double sampleRate) noexcept;
    
private:
    // Engine configuration and state
    ReconfigManager reconfigManager_;
    std::atomic<bool> initialized_{false};
    std::atomic<uint64_t> currentFrameId_{0};
    std::atomic<uint64_t> processedSamples_{0};
    
    // Audio processing state
    double sampleRate_ = 44100.0;
    int currentBlockSize_ = 512;
    int currentFFTSize_ = 512;
    int currentHopSize_ = 128;
    int currentNumBins_ = 257;
    
    // RT-safe processing methods
    void performSTFT(const float* input, int numSamples) noexcept;
    void performSpectralProcessing(const ParameterSnapshot& params) noexcept;
    void performiFFT(float* output, int numSamples) noexcept;
    void updateTelemetry() noexcept;
    
    // Pre-allocated RT-safe buffers (sized for max FFT)
    static constexpr int MAX_FFT_SIZE = 2048;
    static constexpr int MAX_BINS = MAX_FFT_SIZE / 2 + 1;
    static constexpr int MAX_OVERLAP_SIZE = MAX_FFT_SIZE * 4;
    
    AlignedStorage<std::array<float, MAX_FFT_SIZE>> fftInputBuffer_;
    AlignedStorage<std::array<float, MAX_FFT_SIZE>> fftOutputBuffer_;
    AlignedStorage<std::array<float, MAX_BINS>> magnitudeBuffer_;
    AlignedStorage<std::array<float, MAX_BINS>> phaseBuffer_;
    AlignedStorage<std::array<float, MAX_BINS>> previousPhase_;
    AlignedStorage<std::array<float, MAX_BINS>> currentMask_;
    
    // Overlap-add processing
    RTCircularBuffer<float, MAX_OVERLAP_SIZE> inputRingBuffer_;
    RTCircularBuffer<float, MAX_OVERLAP_SIZE> outputRingBuffer_;
    
    // Synthesis state
    enum class SynthMode : uint8_t { Synth = 0, Resynth = 1, Hybrid = 2 };
    std::atomic<SynthMode> currentSynthMode_{SynthMode::Synth};
    
    // Oscillator bank for synthesis mode
    AlignedStorage<std::array<float, MAX_BINS>> oscPhases_;
    AlignedStorage<std::array<float, MAX_BINS>> oscIncrements_;
    
    // Sample data for resynthesis mode (allocated once)
    std::unique_ptr<float[]> sampleData_;
    std::atomic<int> sampleLength_{0};
    std::atomic<double> sampleSampleRate_{44100.0};
    std::atomic<float> samplePlaybackPos_{0.0f};
    
    // Telemetry state
    mutable std::atomic<float> cpuLoadPercent_{0.0f};
    mutable std::atomic<int> activeBinCount_{0};
    mutable std::atomic<float> peakMagnitude_{0.0f};
    mutable std::atomic<float> spectralCentroid_{0.0f};
    mutable std::atomic<uint32_t> maskColumnsProcessed_{0};
    mutable std::atomic<uint32_t> maskColumnsDropped_{0};
    mutable std::atomic<bool> rtViolation_{false};
    
    // Helper methods
    void resetSpectralState() noexcept;
    void updateSpectralFeatures() noexcept;
    void applySynthMode(const ParameterSnapshot& params) noexcept;
    void applyResynthMode(const ParameterSnapshot& params) noexcept;
    void applyHybridMode(const ParameterSnapshot& params) noexcept;
    
    // RT-safe mask interpolation
    void interpolateMask(const float* targetMask, uint32_t numBins, 
                        float smoothingFactor) noexcept;
    
    // CPU load measurement
    class ScopedLoadMeasurer
    {
    public:
        ScopedLoadMeasurer(std::atomic<float>& loadPercent, int blockSize, double sampleRate) noexcept
            : loadPercent_(loadPercent), blockSize_(blockSize), sampleRate_(sampleRate)
        {
            startTime_ = std::chrono::high_resolution_clock::now();
        }
        
        ~ScopedLoadMeasurer() noexcept
        {
            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime_);
            
            // Calculate percentage of available time used
            double blockTimeMs = (blockSize_ / sampleRate_) * 1000.0;
            double usedTimeMs = duration.count() / 1000.0;
            float loadPercent = static_cast<float>((usedTimeMs / blockTimeMs) * 100.0);
            
            loadPercent_.store(loadPercent, std::memory_order_relaxed);
        }
        
    private:
        std::atomic<float>& loadPercent_;
        int blockSize_;
        double sampleRate_;
        std::chrono::high_resolution_clock::time_point startTime_;
    };
};

/**
 * Pure function interface for engine commands
 * These can be easily tested without engine state
 */
namespace SpectralCoreOps
{
    // Pure spectral processing functions
    void computeMagnitudeSpectrum(const float* fftData, float* magnitudes, int numBins) noexcept;
    void computePhaseSpectrum(const float* fftData, float* phases, int numBins) noexcept;
    void applySpectralMask(float* magnitudes, const float* mask, int numBins, float depth) noexcept;
    void synthesizeFromSpectrum(const float* magnitudes, const float* phases, 
                               float* fftData, int fftSize) noexcept;
    
    // Spectral analysis functions
    float computeSpectralCentroid(const float* magnitudes, int numBins, float binFreq) noexcept;
    float findPeakMagnitude(const float* magnitudes, int numBins) noexcept;
    int countActiveBins(const float* magnitudes, int numBins, float threshold) noexcept;
    
    // Mask interpolation
    void smoothMask(const float* targetMask, float* currentMask, int numBins, 
                   float smoothingFactor) noexcept;
};