/******************************************************************************
 * File: PerformanceProfiler.h
 * Description: Performance profiling and monitoring for SpectralCanvas Pro
 * 
 * Tracks paint-to-audio pipeline latency and identifies bottlenecks.
 * Essential for maintaining sub-10ms response times.
 * 
 * Copyright (c) 2025 Spectral Audio Systems
 ******************************************************************************/

#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <chrono>
#include <vector>
#include <string>
#include <unordered_map>

/**
 * @brief High-precision performance profiler for real-time audio systems
 * 
 * Features:
 * - Microsecond-precision timing
 * - Thread-safe operation for audio threads
 * - Statistical analysis (min, max, average, percentiles)
 * - Bottleneck identification
 * - Real-time monitoring dashboard
 */
class PerformanceProfiler
{
public:
    PerformanceProfiler();
    ~PerformanceProfiler();
    
    //==============================================================================
    // Core Profiling Interface
    
    /**
     * @brief RAII timer for automatic timing of code blocks
     */
    class ScopedTimer
    {
    public:
        ScopedTimer(PerformanceProfiler& profiler, const std::string& name);
        ~ScopedTimer();
        
    private:
        PerformanceProfiler& profiler;
        std::string timerName;
        std::chrono::high_resolution_clock::time_point startTime;
    };
    
    // Manual timing interface
    void startTimer(const std::string& name);
    void endTimer(const std::string& name);
    
    // Record a specific duration
    void recordTiming(const std::string& name, double microseconds);
    
    //==============================================================================
    // Paint-to-Audio Pipeline Monitoring
    
    enum class PipelineStage
    {
        PaintCapture,           // Paint event capture and processing
        SpatialGridLookup,      // Grid coordinate calculation and lookup
        ParameterMapping,       // Paint data to audio parameter mapping
        SampleSelection,        // Sample slot selection and loading
        AudioProcessing,        // Audio synthesis and effects
        BufferOutput,           // Audio buffer preparation and output
        TotalLatency           // End-to-end paint-to-audio latency
    };
    
    // Convenient pipeline stage timing
    void startPipelineStage(PipelineStage stage);
    void endPipelineStage(PipelineStage stage);
    
    // SUB-5MS OPTIMIZATION: Specialized paint-to-audio profiling methods
    void recordPaintToAudioLatency(double latencyUs);
    void recordOscillatorAllocation(double allocationTimeUs);
    void recordSpatialGridLookup(double lookupTimeUs);
    double getCurrentPaintToAudioLatency() const;
    bool isPaintToAudioWithinTarget() const;
    
    // Record complete pipeline execution
    struct PipelineExecution
    {
        double paintCapture_us = 0.0;
        double spatialGridLookup_us = 0.0;
        double parameterMapping_us = 0.0;
        double sampleSelection_us = 0.0;
        double audioProcessing_us = 0.0;
        double bufferOutput_us = 0.0;
        double totalLatency_us = 0.0;
        
        bool meetsLatencyTarget(double targetMicroseconds = 10000.0) const
        {
            return totalLatency_us <= targetMicroseconds;
        }
    };
    
    void recordPipelineExecution(const PipelineExecution& execution);
    
    //==============================================================================
    // Performance Statistics
    
    struct TimingStats
    {
        std::string name;
        
        // Basic statistics
        uint64_t sampleCount = 0;
        double totalTime_us = 0.0;
        double minTime_us = std::numeric_limits<double>::max();
        double maxTime_us = 0.0;
        double averageTime_us = 0.0;
        
        // Advanced statistics
        double median_us = 0.0;
        double percentile95_us = 0.0;
        double percentile99_us = 0.0;
        double standardDeviation_us = 0.0;
        
        // Performance indicators
        bool exceedsTarget = false;
        double targetTime_us = 10000.0; // 10ms default
        uint64_t exceedCount = 0;
        
        void updateFromSamples(const std::vector<double>& samples);
    };
    
    TimingStats getTimingStats(const std::string& name) const;
    std::vector<TimingStats> getAllTimingStats() const;
    
    // Pipeline-specific statistics
    TimingStats getPipelineStats(PipelineStage stage) const;
    std::vector<TimingStats> getAllPipelineStats() const;
    
    //==============================================================================
    // Real-Time Monitoring
    
    struct PerformanceSnapshot
    {
        std::chrono::system_clock::time_point timestamp;
        
        // Current performance metrics
        double currentCpuUsage = 0.0;           // CPU usage percentage
        double currentMemoryUsage = 0.0;        // Memory usage in MB
        uint32_t activePaintStrokes = 0;        // Active paint strokes
        uint32_t activeSampleVoices = 0;        // Active sample voices
        
        // Recent performance
        double recentAverageLatency_us = 0.0;   // Last 100 samples
        double recentMaxLatency_us = 0.0;       // Maximum in last 100 samples
        uint32_t recentDropouts = 0;            // Dropouts in last second
        
        // System health indicators
        bool isHealthy() const
        {
            return recentAverageLatency_us < 10000.0 &&  // Under 10ms
                   currentCpuUsage < 80.0 &&              // Under 80% CPU
                   recentDropouts == 0;                   // No dropouts
        }
    };
    
    PerformanceSnapshot getCurrentSnapshot() const;
    
    // Alert system for performance issues
    struct PerformanceAlert
    {
        enum class Severity { Info, Warning, Critical };
        
        Severity severity;
        std::string message;
        std::chrono::system_clock::time_point timestamp;
        std::string category; // "latency", "cpu", "memory", etc.
    };
    
    std::vector<PerformanceAlert> getRecentAlerts() const;
    void clearAlerts();
    
    //==============================================================================
    // Configuration & Control
    
    // SUB-5MS OPTIMIZATION: Ultra-precise latency targeting
    void setLatencyTarget(double microseconds) { latencyTarget.store(microseconds); }
    double getLatencyTarget() const { return latencyTarget.load(); }
    
    // FAST PATH: Set sub-5ms target for SpectralCanvas Pro
    void setSubFiveMsTarget() { latencyTarget.store(5000.0); }  // 5ms in microseconds
    bool isWithinSubFiveMsTarget(double latencyUs) const { return latencyUs <= 5000.0; }
    
    void enableProfiling(bool enabled) { profilingEnabled.store(enabled); }
    bool isProfilingEnabled() const { return profilingEnabled.load(); }
    
    void enableDetailedProfiling(bool enabled) { detailedProfiling.store(enabled); }
    bool isDetailedProfilingEnabled() const { return detailedProfiling.load(); }
    
    void setMaxSampleHistory(uint32_t maxSamples) { maxSampleHistory = maxSamples; }
    uint32_t getMaxSampleHistory() const { return maxSampleHistory; }
    
    // Reset all statistics
    void reset();
    void resetPipelineStats();
    
    //==============================================================================
    // Export & Reporting
    
    // Export performance data for analysis
    struct PerformanceReport
    {
        std::chrono::system_clock::time_point reportTime;
        std::vector<TimingStats> timingStats;
        std::vector<TimingStats> pipelineStats;
        std::vector<PerformanceAlert> alerts;
        PerformanceSnapshot currentSnapshot;
        
        // Summary metrics
        double overallHealthScore = 0.0;        // 0.0-1.0, 1.0 = perfect
        bool meetsPerformanceRequirements = false;
        std::string recommendations;
    };
    
    PerformanceReport generateReport() const;
    bool exportReportToFile(const juce::File& outputFile) const;
    std::string generateTextReport() const;
    
    //==============================================================================
    // Integration Helpers
    
    // Macros for easy integration
    #define PROFILE_SCOPE(profiler, name) \
        auto scopedTimer = profiler.createScopedTimer(name)
    
    #define PROFILE_PIPELINE_STAGE(profiler, stage) \
        profiler.startPipelineStage(stage); \
        juce::ScopedValueSetter<bool> stageGuard(dummyBool, true, [&] { profiler.endPipelineStage(stage); })
    
    ScopedTimer createScopedTimer(const std::string& name);
    
private:
    //==============================================================================
    // Data Storage
    
    struct TimingData
    {
        std::vector<double> samples;
        std::chrono::high_resolution_clock::time_point lastUpdate;
        std::atomic<uint64_t> totalSamples{0};
        
        void addSample(double microseconds);
        void trimToMaxSize(uint32_t maxSize);
    };
    
    // RT-SAFE: Use lock-free atomic operations instead of critical sections
    std::atomic<double> recentLatency{0.0};
    std::atomic<double> recentMaxLatency{0.0};
    std::atomic<uint64_t> recentSampleCount{0};
    std::atomic<bool> latencyTargetExceeded{false};
    
    // For non-RT thread access only - marked as such for clarity
    mutable std::mutex timingDataMutex; // NON_RT: Only for reporting thread
    std::unordered_map<std::string, std::unique_ptr<TimingData>> timingData;
    
    // Pipeline-specific data - RT-safe atomic snapshot
    std::array<std::atomic<double>, 7> pipelineLatencies; // One for each PipelineStage
    
    // RT-SAFE: Lock-free timer tracking using thread_local storage
    struct ActiveTimer
    {
        std::chrono::high_resolution_clock::time_point startTime;
        std::string name;
        bool active = false;
    };
    
    // Thread-local storage for RT-safe timer tracking
    static thread_local std::unordered_map<std::string, ActiveTimer> tlsActiveTimers;
    
    //==============================================================================
    // Configuration
    
    std::atomic<double> latencyTarget{10000.0}; // 10ms in microseconds
    std::atomic<bool> profilingEnabled{true};
    std::atomic<bool> detailedProfiling{false};
    uint32_t maxSampleHistory = 1000;
    
    //==============================================================================
    // Alert System - RT-SAFE
    
    // RT-SAFE: Simple atomic alert counters instead of complex alert storage
    std::atomic<uint32_t> criticalAlertCount{0};
    std::atomic<uint32_t> warningAlertCount{0};
    std::atomic<uint32_t> infoAlertCount{0};
    
    // NON_RT: Full alert storage for reporting thread only
    mutable std::mutex alertsMutex; // NON_RT: Only for reporting thread
    std::vector<PerformanceAlert> recentAlerts;
    std::chrono::system_clock::time_point lastAlertCheck;
    
    void checkPerformanceAlerts(const TimingStats& stats);
    void addAlert(PerformanceAlert::Severity severity, const std::string& message, const std::string& category);
    
    //==============================================================================
    // Utility Methods
    
    std::string pipelineStageToString(PipelineStage stage) const;
    TimingData* getOrCreateTimingData(const std::string& name);
    void updateTimingStats(TimingStats& stats, const std::vector<double>& samples) const;
    double calculateHealthScore() const;
    
    // Thread-safe sample addition
    void addTimingSample(const std::string& name, double microseconds);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PerformanceProfiler)
};

//==============================================================================
// Convenience Macros for Easy Integration

#define SPECTRAL_PROFILE_SCOPE(name) \
    auto JUCE_JOIN_MACRO(scopedTimer, __LINE__) = PerformanceProfiler::getInstance().createScopedTimer(name)

#define SPECTRAL_PROFILE_PIPELINE(stage) \
    PerformanceProfiler::getInstance().startPipelineStage(PerformanceProfiler::PipelineStage::stage); \
    juce::ScopedValueSetter<bool> JUCE_JOIN_MACRO(pipelineGuard, __LINE__)(dummyBool, true, \
        [&] { PerformanceProfiler::getInstance().endPipelineStage(PerformanceProfiler::PipelineStage::stage); })

#define SPECTRAL_RECORD_TIMING(name, microseconds) \
    PerformanceProfiler::getInstance().recordTiming(name, microseconds)

// Global profiler instance
class PerformanceProfiler;
extern PerformanceProfiler& getGlobalProfiler();