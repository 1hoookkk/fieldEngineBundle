#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <array>
#include <chrono>
#include <memory>
#include <string>

/**
 * ContinuousVerification - Pattern #7 implementation
 * 
 * Provides runtime monitoring, RT-safety validation, and performance telemetry
 * to catch regressions early and maintain architectural invariants.
 */
class ContinuousVerification
{
public:
    // Performance telemetry data
    struct PerformanceTelemetry
    {
        float cpuLoadPercent = 0.0f;          // Current CPU usage
        float peakCpuLoadPercent = 0.0f;      // Peak CPU usage
        int xrunCount = 0;                    // Buffer underrun count
        int allocCount = 0;                   // RT allocations detected
        float avgBlockTimeMs = 0.0f;          // Average block processing time
        float maxBlockTimeMs = 0.0f;          // Maximum block processing time
        int queueDepthAudioToUI = 0;          // Spectral data queue depth
        int queueDepthUIToAudio = 0;          // Parameter queue depth
        int queueDepthGPUToAudio = 0;         // Mask queue depth
        int dropCountAudioToUI = 0;           // Dropped spectral frames
        int dropCountUIToAudio = 0;           // Dropped parameter updates
        int dropCountGPUToAudio = 0;          // Dropped mask columns
        uint64_t totalSamplesProcessed = 0;   // Cumulative samples
        bool rtViolationDetected = false;     // RT-safety violation flag
    };
    
    ContinuousVerification() noexcept = default;
    ~ContinuousVerification() = default;
    
    // Initialize with audio parameters
    void initialize(double sampleRate, int maxBlockSize) noexcept;
    
    // RT-safe performance measurement
    class ScopedBlockTimer
    {
    public:
        ScopedBlockTimer(ContinuousVerification& verifier, int blockSize) noexcept
            : verifier_(verifier), blockSize_(blockSize)
        {
            startTime_ = std::chrono::high_resolution_clock::now();
        }
        
        ~ScopedBlockTimer() noexcept
        {
            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime_);
            verifier_.recordBlockTime(blockSize_, duration.count());
        }
        
    private:
        ContinuousVerification& verifier_;
        int blockSize_;
        std::chrono::high_resolution_clock::time_point startTime_;
    };
    
    // Record performance data (RT-safe)
    void recordBlockTime(int blockSize, int64_t microseconds) noexcept;
    void recordXrun() noexcept { xrunCount_.fetch_add(1, std::memory_order_relaxed); }
    void recordAllocation() noexcept;
    void recordSamplesProcessed(int numSamples) noexcept;
    
    // Queue monitoring (called from UI thread)
    void updateQueueMetrics(int audioToUI, int uiToAudio, int gpuToAudio,
                          int dropsAudioToUI, int dropsUIToAudio, int dropsGPUToAudio) noexcept;
    
    // Get current telemetry (thread-safe)
    PerformanceTelemetry getTelemetry() const noexcept;
    
    // Reset counters
    void reset() noexcept;
    
    // RT-safety validation helpers
    static bool isAudioThread() noexcept;
    static void assertAudioThread() noexcept;
    static void assertNotAudioThread() noexcept;
    
private:
    // Performance tracking
    double sampleRate_ = 44100.0;
    int maxBlockSize_ = 512;
    
    // Atomic counters (RT-safe)
    std::atomic<int> xrunCount_{0};
    std::atomic<int> allocCount_{0};
    std::atomic<uint64_t> totalSamples_{0};
    std::atomic<bool> rtViolation_{false};
    
    // Block timing (moving average)
    static constexpr int TIMING_HISTORY_SIZE = 32;
    std::array<std::atomic<float>, TIMING_HISTORY_SIZE> blockTimes_{};
    std::atomic<int> timingIndex_{0};
    std::atomic<float> peakBlockTime_{0.0f};
    
    // Queue metrics (updated from UI thread)
    mutable std::atomic<int> queueDepthAudioToUI_{0};
    mutable std::atomic<int> queueDepthUIToAudio_{0};
    mutable std::atomic<int> queueDepthGPUToAudio_{0};
    mutable std::atomic<int> dropCountAudioToUI_{0};
    mutable std::atomic<int> dropCountUIToAudio_{0};
    mutable std::atomic<int> dropCountGPUToAudio_{0};
    
    // Helper methods
    float calculateCPULoad(float blockTimeMs) const noexcept;
    float getAverageBlockTime() const noexcept;
};

/**
 * RT-safe allocation detector
 * Hooks into malloc/new to detect allocations on audio thread
 */
class AllocationDetector
{
public:
    static AllocationDetector& getInstance() noexcept;
    
    void enable() noexcept { enabled_.store(true, std::memory_order_release); }
    void disable() noexcept { enabled_.store(false, std::memory_order_release); }
    bool isEnabled() const noexcept { return enabled_.load(std::memory_order_acquire); }
    
    void recordAllocation(size_t size) noexcept;
    int getAllocationCount() const noexcept { return allocCount_.load(std::memory_order_relaxed); }
    void resetCount() noexcept { allocCount_.store(0, std::memory_order_relaxed); }
    
private:
    std::atomic<bool> enabled_{false};
    std::atomic<int> allocCount_{0};
    
    AllocationDetector() = default;
};

/**
 * RT-safety assertions for debug builds
 * Note: RT_ASSERT_AUDIO_THREAD is now defined in RealtimeSafeTypes.h to avoid redefinition
 */
#ifdef JUCE_DEBUG
    #define RT_ASSERT_NOT_AUDIO_THREAD() ContinuousVerification::assertNotAudioThread()
    #define RT_SAFE_SCOPE_TIMER(verifier, blockSize) \
        ContinuousVerification::ScopedBlockTimer timer(verifier, blockSize)
#else
    #define RT_ASSERT_NOT_AUDIO_THREAD() ((void)0)
    #define RT_SAFE_SCOPE_TIMER(verifier, blockSize) ((void)0)
#endif

/**
 * Golden-master audio test utilities
 * For validating STFT/iFFT and paint-to-audio consistency
 */
class AudioTestFramework
{
public:
    struct TestResult
    {
        bool passed = false;
        float maxError = 0.0f;
        float rmsError = 0.0f;
        std::string description;
    };
    
    // Test STFT round-trip accuracy
    static TestResult testSTFTRoundTrip(int fftSize, int hopSize, int numSamples);
    
    // Test paint-to-audio consistency
    static TestResult testPaintToAudio(const float* inputAudio, int numSamples,
                                     const float* maskData, int numBins);
    
    // Test parameter smoothing
    static TestResult testParameterSmoothing(float startValue, float endValue, 
                                           int numSamples, float sampleRate);
    
    // Generate reference test signals
    static void generateSineWave(float* buffer, int numSamples, float frequency, 
                                float sampleRate, float amplitude = 1.0f);
    static void generateWhiteNoise(float* buffer, int numSamples, float amplitude = 1.0f);
    static void generateSilence(float* buffer, int numSamples);
    
    // Audio comparison utilities
    static float calculateRMSError(const float* signal1, const float* signal2, int numSamples);
    static float calculateMaxError(const float* signal1, const float* signal2, int numSamples);
    static bool compareAudioBuffers(const float* expected, const float* actual, 
                                  int numSamples, float tolerance = 1e-6f);
    
private:
    static constexpr float PI = 3.14159265359f;
    static constexpr float TWO_PI = 2.0f * PI;
};

/**
 * Pluginval integration helper
 * Prepares plugin for automated testing with strict validation
 */
class PluginvalIntegration
{
public:
    // Configure plugin for pluginval testing
    static void prepareForValidation();
    
    // Enable strict RT-safety checking
    static void enableStrictRTChecking();
    
    // Disable features that might interfere with validation
    static void disableProblematicFeatures();
    
    // Generate pluginval command line
    static std::string generatePluginvalCommand(const std::string& pluginPath,
                                              int strictnessLevel = 8,
                                              int timeoutSeconds = 300);
    
    // Parse pluginval results
    struct ValidationResult
    {
        bool passed = false;
        int numTests = 0;
        int numPassed = 0;
        int numFailed = 0;
        std::vector<std::string> failures;
        std::string summary;
    };
    
    static ValidationResult parseResults(const std::string& logOutput);
};

/**
 * Performance profiler for hot paths
 * Tracks function call counts and timing in development builds
 */
class PerformanceProfiler
{
public:
    struct ProfileData
    {
        uint64_t callCount = 0;
        uint64_t totalTimeUs = 0;
        uint64_t maxTimeUs = 0;
        uint64_t minTimeUs = UINT64_MAX;
        std::string functionName;
        
        double getAverageTimeUs() const 
        { 
            return callCount > 0 ? static_cast<double>(totalTimeUs) / callCount : 0.0; 
        }
    };
    
    // RAII profiler for function timing
    class ScopedProfiler
    {
    public:
        ScopedProfiler(const char* functionName) noexcept;
        ~ScopedProfiler() noexcept;
        
    private:
        const char* functionName_;
        std::chrono::high_resolution_clock::time_point startTime_;
    };
    
    static PerformanceProfiler& getInstance() noexcept;
    
    void recordCall(const char* functionName, uint64_t timeUs) noexcept;
    std::vector<ProfileData> getProfiles() const;
    void reset() noexcept;
    void setEnabled(bool enabled) noexcept { enabled_.store(enabled, std::memory_order_release); }
    
private:
    std::atomic<bool> enabled_{false};
    // Profile data storage would go here (simplified for brevity)
};

// Macro for easy profiling in development
#ifdef JUCE_DEBUG
    #define PROFILE_SCOPE() PerformanceProfiler::ScopedProfiler prof(__FUNCTION__)
    #define PROFILE_NAMED_SCOPE(name) PerformanceProfiler::ScopedProfiler prof(name)
#else
    #define PROFILE_SCOPE() ((void)0)
    #define PROFILE_NAMED_SCOPE(name) ((void)0)
#endif