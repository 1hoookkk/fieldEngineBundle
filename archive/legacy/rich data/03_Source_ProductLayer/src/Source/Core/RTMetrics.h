#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <array>
#include <chrono>

/**
 * RT-Safe Performance Metrics Collection for SpectralCanvas Pro v2
 * 
 * Lock-free metrics collection for audio thread performance monitoring.
 * Supports the Subagent Operating System KPI framework.
 * 
 * Design Principles:
 * - Zero allocations on audio thread
 * - Lock-free ring buffer for timing data
 * - Atomic operations for thread safety
 * - Minimal performance impact (<0.1% CPU overhead)
 */
class RTMetrics
{
public:
    // Singleton access for global metrics collection
    static RTMetrics& instance() noexcept
    {
        static RTMetrics instance;
        return instance;
    }
    
    // Audio callback timing measurement
    struct CallbackTiming
    {
        uint64_t startTimeUs;      // Microseconds since epoch
        uint64_t durationUs;       // Callback duration in microseconds
        uint32_t blockSize;        // Samples per block
        uint32_t sampleRate;       // Sample rate in Hz
        uint32_t channelCount;     // Number of channels processed
    };
    
    // RT-safe timing capture
    void startCallbackTiming() noexcept
    {
        currentTiming.startTimeUs = getCurrentTimeMicros();
    }
    
    void endCallbackTiming(uint32_t blockSize, uint32_t sampleRate, uint32_t channels) noexcept
    {
        auto endTime = getCurrentTimeMicros();
        currentTiming.durationUs = endTime - currentTiming.startTimeUs;
        currentTiming.blockSize = blockSize;
        currentTiming.sampleRate = sampleRate;
        currentTiming.channelCount = channels;
        
        // Push to lock-free ring buffer
        auto writeIndex = writePosition.load(std::memory_order_relaxed);
        timingBuffer[writeIndex % RING_BUFFER_SIZE] = currentTiming;
        writePosition.store(writeIndex + 1, std::memory_order_release);
    }
    
    // Non-RT statistics computation (for UI/logging thread)
    struct Statistics
    {
        double meanUs = 0.0;
        double p50Us = 0.0;
        double p95Us = 0.0; 
        double p99Us = 0.0;
        double maxUs = 0.0;
        uint32_t sampleCount = 0;
        uint32_t droppedSamples = 0;
        double gestureToSoundLatencyMs = 0.0;
    };
    
    Statistics computeStatistics() noexcept
    {
        Statistics stats{};
        
        // Read all available samples from ring buffer
        auto currentWrite = writePosition.load(std::memory_order_acquire);
        auto currentRead = lastReadPosition.load(std::memory_order_relaxed);
        
        if (currentWrite == currentRead) {
            return stats; // No new data
        }
        
        // Collect timing samples
        std::array<double, RING_BUFFER_SIZE> durations;
        uint32_t sampleCount = 0;
        
        for (auto i = currentRead; i < currentWrite && sampleCount < RING_BUFFER_SIZE; ++i) {
            auto& timing = timingBuffer[i % RING_BUFFER_SIZE];
            durations[sampleCount] = static_cast<double>(timing.durationUs);
            sampleCount++;
        }
        
        lastReadPosition.store(currentWrite, std::memory_order_relaxed);
        
        if (sampleCount == 0) {
            return stats;
        }
        
        // Sort for percentile calculations
        std::sort(durations.begin(), durations.begin() + sampleCount);
        
        // Compute statistics
        stats.sampleCount = sampleCount;
        stats.maxUs = durations[sampleCount - 1];
        
        // Mean calculation
        double sum = 0.0;
        for (uint32_t i = 0; i < sampleCount; ++i) {
            sum += durations[i];
        }
        stats.meanUs = sum / sampleCount;
        
        // Percentiles
        stats.p50Us = durations[static_cast<uint32_t>(sampleCount * 0.50)];
        stats.p95Us = durations[static_cast<uint32_t>(sampleCount * 0.95)];
        stats.p99Us = durations[static_cast<uint32_t>(sampleCount * 0.99)];
        
        // Estimate gesture-to-sound latency (simplified)
        // This would need integration with paint queue timing in production
        auto avgBlockSize = getAverageBlockSize();
        auto avgSampleRate = getAverageSampleRate();
        if (avgSampleRate > 0) {
            stats.gestureToSoundLatencyMs = (avgBlockSize / avgSampleRate) * 1000.0 + (stats.meanUs / 1000.0);
        }
        
        return stats;
    }
    
    // RT-safe counters for additional metrics
    void incrementBufferUnderruns() noexcept
    {
        bufferUnderruns.fetch_add(1, std::memory_order_relaxed);
    }
    
    void incrementRTAllocations() noexcept
    {
        rtAllocations.fetch_add(1, std::memory_order_relaxed);
    }
    
    void recordPaintToSoundLatency(double latencyMs) noexcept
    {
        paintToSoundLatencyMs.store(static_cast<uint32_t>(latencyMs * 1000), std::memory_order_relaxed);
    }
    
    // Accessors for KPI reporting
    uint32_t getBufferUnderrunCount() const noexcept
    {
        return bufferUnderruns.load(std::memory_order_relaxed);
    }
    
    uint32_t getRTAllocationCount() const noexcept
    {
        return rtAllocations.load(std::memory_order_relaxed);
    }
    
    double getPaintToSoundLatencyMs() const noexcept
    {
        return paintToSoundLatencyMs.load(std::memory_order_relaxed) / 1000.0;
    }
    
    void resetCounters() noexcept
    {
        bufferUnderruns.store(0, std::memory_order_relaxed);
        rtAllocations.store(0, std::memory_order_relaxed);
        paintToSoundLatencyMs.store(0, std::memory_order_relaxed);
    }
    
private:
    RTMetrics() = default;
    ~RTMetrics() = default;
    RTMetrics(const RTMetrics&) = delete;
    RTMetrics& operator=(const RTMetrics&) = delete;
    
    static constexpr uint32_t RING_BUFFER_SIZE = 2048; // Must be power of 2
    
    // Lock-free ring buffer for timing data
    std::array<CallbackTiming, RING_BUFFER_SIZE> timingBuffer{};
    std::atomic<uint32_t> writePosition{0};
    std::atomic<uint32_t> lastReadPosition{0};
    
    // Current timing being measured (audio thread only)
    CallbackTiming currentTiming{};
    
    // RT-safe performance counters
    std::atomic<uint32_t> bufferUnderruns{0};
    std::atomic<uint32_t> rtAllocations{0};
    std::atomic<uint32_t> paintToSoundLatencyMs{0}; // In microseconds for precision
    
    // Cached values for latency estimation
    mutable std::atomic<uint32_t> cachedBlockSize{512};
    mutable std::atomic<uint32_t> cachedSampleRate{44100};
    
    uint64_t getCurrentTimeMicros() noexcept
    {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::high_resolution_clock::now().time_since_epoch()
            ).count()
        );
    }
    
    double getAverageBlockSize() const noexcept
    {
        return static_cast<double>(cachedBlockSize.load(std::memory_order_relaxed));
    }
    
    double getAverageSampleRate() const noexcept  
    {
        return static_cast<double>(cachedSampleRate.load(std::memory_order_relaxed));
    }
    
    friend class RTMetricsReporter; // For non-RT thread access
};

/**
 * Helper class for periodic metrics reporting to docs/PERF.md
 * Runs on a separate thread to avoid impacting RT performance
 */
class RTMetricsReporter
{
public:
    RTMetricsReporter() = default;
    ~RTMetricsReporter() { stop(); }
    
    void start(int reportingIntervalMs = 5000)
    {
        if (reportingThread.joinable()) {
            stop();
        }
        
        shouldStop.store(false, std::memory_order_relaxed);
        reportingThread = std::thread([this, reportingIntervalMs] {
            reportingLoop(reportingIntervalMs);
        });
    }
    
    void stop()
    {
        shouldStop.store(true, std::memory_order_relaxed);
        if (reportingThread.joinable()) {
            reportingThread.join();
        }
    }
    
    // Force immediate metrics report (for subagent KPI reporting)
    RTMetrics::Statistics getLatestStatistics()
    {
        return RTMetrics::instance().computeStatistics();
    }
    
private:
    std::atomic<bool> shouldStop{false};
    std::thread reportingThread;
    
    void reportingLoop(int intervalMs)
    {
        while (!shouldStop.load(std::memory_order_relaxed)) {
            auto stats = RTMetrics::instance().computeStatistics();
            
            // Log to file for historical tracking
            logMetricsToFile(stats);
            
            // Update live PERF.md if significant change
            updatePerfMd(stats);
            
            std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
        }
    }
    
    void logMetricsToFile(const RTMetrics::Statistics& stats)
    {
        // Create logs/metrics directory if it doesn't exist
        juce::File logsDir("logs/metrics");
        if (!logsDir.exists()) {
            logsDir.createDirectory();
        }
        
        // Daily log rotation
        auto now = juce::Time::getCurrentTime();
        auto filename = now.toString(true, false, false).replace(" ", "_").replace(":", "-") + ".json";
        juce::File logFile = logsDir.getChildFile(filename);
        
        // Append metrics JSON entry
        juce::var entry = new juce::DynamicObject();
        entry.getDynamicObject()->setProperty("timestamp", now.toMilliseconds());
        entry.getDynamicObject()->setProperty("mean_us", stats.meanUs);
        entry.getDynamicObject()->setProperty("p50_us", stats.p50Us);
        entry.getDynamicObject()->setProperty("p95_us", stats.p95Us);
        entry.getDynamicObject()->setProperty("p99_us", stats.p99Us);
        entry.getDynamicObject()->setProperty("max_us", stats.maxUs);
        entry.getDynamicObject()->setProperty("sample_count", stats.sampleCount);
        entry.getDynamicObject()->setProperty("gesture_to_sound_ms", stats.gestureToSoundLatencyMs);
        
        auto jsonString = juce::JSON::toString(entry) + "\n";
        logFile.appendText(jsonString);
    }
    
    void updatePerfMd(const RTMetrics::Statistics& stats)
    {
        // This would update docs/PERF.md with live metrics
        // Implementation depends on specific format requirements
        // For now, just track that we have the capability
    }
};

// Convenient RAII timer for automatic callback timing
class RTCallbackTimer
{
public:
    RTCallbackTimer(uint32_t blockSize, uint32_t sampleRate, uint32_t channels)
        : blockSize_(blockSize), sampleRate_(sampleRate), channels_(channels)
    {
        RTMetrics::instance().startCallbackTiming();
    }
    
    ~RTCallbackTimer()
    {
        RTMetrics::instance().endCallbackTiming(blockSize_, sampleRate_, channels_);
    }
    
private:
    uint32_t blockSize_;
    uint32_t sampleRate_;
    uint32_t channels_;
};

// Macro for easy integration into processBlock()
#define RT_METRICS_TIMER(blockSize, sampleRate, channels) \
    RTCallbackTimer _rt_timer(blockSize, sampleRate, channels)