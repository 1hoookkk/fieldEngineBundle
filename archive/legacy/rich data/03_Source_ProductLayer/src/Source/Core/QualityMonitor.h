/******************************************************************************
 * File: QualityMonitor.h
 * Description: Permanent quality monitoring and performance tracking system
 * 
 * Code Quality Guardian - Continuous system health monitoring
 * 
 * Copyright (c) 2025 Spectral Audio Systems
 ******************************************************************************/

#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

//==============================================================================
// Performance Metrics Collection
//==============================================================================

struct PerformanceMetrics
{
    // CPU Usage Metrics
    std::atomic<float> cpuUsagePercent{0.0f};
    std::atomic<float> peakCpuUsage{0.0f};
    std::atomic<int> cpuSpikes{0}; // Count of >80% usage spikes
    
    // Memory Usage Metrics  
    std::atomic<size_t> memoryUsageMB{0};
    std::atomic<size_t> peakMemoryUsage{0};
    std::atomic<int> memoryLeaksDetected{0};
    
    // Audio Performance Metrics
    std::atomic<float> audioLatencyMs{0.0f};
    std::atomic<int> audioDropouts{0};
    std::atomic<int> bufferUnderruns{0};
    std::atomic<float> sampleRate{44100.0f};
    
    // Real-time Performance Metrics
    std::atomic<float> paintToAudioLatencyMs{0.0f};
    std::atomic<int> paintEventsMissed{0};
    std::atomic<float> uiFrameRate{60.0f};
    
    // System Health Metrics
    std::atomic<bool> spectralEngineHealthy{false};
    std::atomic<bool> audioDeviceHealthy{false};
    std::atomic<bool> paintSystemHealthy{false};
    std::atomic<int> recoveryEventsCount{0};
    
    // Quality Metrics
    std::atomic<float> overallHealthScore{0.0f}; // 0-100%
    std::atomic<int> criticalErrorsCount{0};
    std::atomic<int> warningsCount{0};
};

//==============================================================================
// Quality Monitoring Events
//==============================================================================

enum class QualityEvent
{
    SystemStartup,
    ComponentInitialized,
    ComponentFailed,
    PerformanceAlert,
    MemoryAlert,
    AudioAlert,
    RecoveryTriggered,
    QualityDegraded,
    SystemShutdown
};

struct QualityEventData
{
    QualityEvent event;
    std::chrono::steady_clock::time_point timestamp;
    std::string component;
    std::string message;
    float severity; // 0.0 = info, 1.0 = critical
    std::unordered_map<std::string, float> metrics;
};

//==============================================================================
// Quality Monitor - Main Class
//==============================================================================

class QualityMonitor : public juce::Timer
{
public:
    QualityMonitor();
    ~QualityMonitor() override;
    
    // Lifecycle Management
    void startMonitoring();
    void stopMonitoring();
    bool isMonitoring() const { return monitoringActive.load(); }
    
    // Metrics Collection
    void updateCpuUsage(float usage);
    void updateMemoryUsage(size_t usageMB);
    void updateAudioLatency(float latencyMs);
    void updatePaintLatency(float latencyMs);
    void reportAudioDropout();
    void reportPaintEventMissed();
    
    // Component Health Tracking
    void reportComponentHealth(const std::string& component, bool healthy);
    void reportComponentInitialized(const std::string& component);
    void reportComponentFailed(const std::string& component, const std::string& error);
    void reportRecoveryEvent(const std::string& component, const std::string& action);
    
    // Quality Assessment
    float calculateOverallHealthScore() const;
    bool isSystemHealthy() const;
    std::vector<std::string> getActiveAlerts() const;
    std::string generateHealthReport() const;
    
    // Performance Thresholds
    void setPerformanceThresholds(float maxCpuPercent = 80.0f,
                                size_t maxMemoryMB = 500,
                                float maxAudioLatencyMs = 10.0f,
                                float maxPaintLatencyMs = 16.0f);
    
    // Event Monitoring
    void addEventListener(std::function<void(const QualityEventData&)> listener);
    void removeAllEventListeners();
    
    // Access to Metrics
    const PerformanceMetrics& getMetrics() const { return metrics; }
    
    // Testing and Diagnostics
    void runDiagnosticTests();
    void simulateStressTest(int durationSeconds);
    void exportMetricsToFile(const juce::File& file) const;

private:
    // Timer Callback
    void timerCallback() override;
    
    // Internal Methods
    void updateSystemHealth();
    void checkPerformanceThresholds();
    void logQualityEvent(const QualityEventData& event);
    void calculateHealthScore();
    void cleanupOldEvents();
    
    // Performance Analysis
    void analyzeCpuTrends();
    void analyzeMemoryTrends();
    void detectPerformanceRegression();
    
    // Alert Management
    void triggerAlert(const std::string& message, float severity);
    void clearAlert(const std::string& alertId);
    
    // Data Members
    PerformanceMetrics metrics;
    
    std::atomic<bool> monitoringActive{false};
    std::chrono::steady_clock::time_point monitoringStartTime;
    
    // Performance Thresholds
    float cpuThreshold{80.0f};
    size_t memoryThreshold{500}; // MB
    float audioLatencyThreshold{10.0f}; // ms
    float paintLatencyThreshold{16.0f}; // ms
    
    // Event Management
    std::vector<QualityEventData> eventHistory;
    std::vector<std::function<void(const QualityEventData&)>> eventListeners;
    
    // Active Alerts
    std::unordered_map<std::string, std::string> activeAlerts;
    
    // Performance Trend Analysis
    std::vector<float> cpuHistory;
    std::vector<size_t> memoryHistory;
    static constexpr size_t MAX_HISTORY_SIZE = 1000;
    
    // Thread Safety
    juce::CriticalSection eventLock;
    juce::CriticalSection alertLock;
    juce::CriticalSection metricsLock;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QualityMonitor)
};

//==============================================================================
// Global Quality Monitor Access
//==============================================================================

// Singleton access to quality monitor
QualityMonitor& getQualityMonitor();

// Convenience macros for common quality monitoring operations
#define QUALITY_REPORT_COMPONENT_INIT(component) \
    getQualityMonitor().reportComponentInitialized(component)

#define QUALITY_REPORT_COMPONENT_FAILED(component, error) \
    getQualityMonitor().reportComponentFailed(component, error)

#define QUALITY_REPORT_PERFORMANCE(cpu, memory, audioLatency) \
    do { \
        auto& monitor = getQualityMonitor(); \
        monitor.updateCpuUsage(cpu); \
        monitor.updateMemoryUsage(memory); \
        monitor.updateAudioLatency(audioLatency); \
    } while(0)

#define QUALITY_CHECK_HEALTH() \
    getQualityMonitor().isSystemHealthy()

#define QUALITY_GET_HEALTH_SCORE() \
    getQualityMonitor().calculateOverallHealthScore()

//==============================================================================
// Quality Assertions - Development/Debug Mode Only
//==============================================================================

#if JUCE_DEBUG
    #define QUALITY_ASSERT(condition, message) \
        do { \
            if (!(condition)) { \
                getQualityMonitor().reportComponentFailed("ASSERTION", message); \
                jassertfalse; \
            } \
        } while(0)
    
    #define QUALITY_ASSERT_PERFORMANCE(latencyMs, maxLatencyMs, component) \
        do { \
            if (latencyMs > maxLatencyMs) { \
                getQualityMonitor().reportComponentFailed(component, \
                    "Performance assertion failed: " + std::to_string(latencyMs) + "ms > " + std::to_string(maxLatencyMs) + "ms"); \
            } \
        } while(0)
#else
    #define QUALITY_ASSERT(condition, message) ((void)0)
    #define QUALITY_ASSERT_PERFORMANCE(latencyMs, maxLatencyMs, component) ((void)0)
#endif

//==============================================================================
// Performance Measurement Helpers
//==============================================================================

class ScopedPerformanceTimer
{
public:
    ScopedPerformanceTimer(const std::string& operation, float alertThresholdMs = 100.0f)
        : operationName(operation)
        , threshold(alertThresholdMs)
        , startTime(std::chrono::high_resolution_clock::now())
    {
    }
    
    ~ScopedPerformanceTimer()
    {
        auto endTime = std::chrono::high_resolution_clock::now();
        auto durationMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();
        
        if (durationMs > threshold)
        {
            getQualityMonitor().reportComponentFailed(operationName, 
                "Performance threshold exceeded: " + std::to_string(durationMs) + "ms");
        }
    }
    
private:
    std::string operationName;
    float threshold;
    std::chrono::high_resolution_clock::time_point startTime;
};

#define QUALITY_MEASURE_PERFORMANCE(operation, thresholdMs) \
    ScopedPerformanceTimer timer##__LINE__(operation, thresholdMs)

#define QUALITY_MEASURE_AUDIO_PERFORMANCE(operation) \
    QUALITY_MEASURE_PERFORMANCE(operation, 5.0f) // 5ms threshold for audio

#define QUALITY_MEASURE_UI_PERFORMANCE(operation) \
    QUALITY_MEASURE_PERFORMANCE(operation, 16.0f) // 16ms threshold for UI