/******************************************************************************
 * File: PerformanceProfiler.cpp
 * Description: Implementation of performance profiling and monitoring
 * 
 * Copyright (c) 2025 Spectral Audio Systems
 ******************************************************************************/

#include "PerformanceProfiler.h"
#include <algorithm>
#include <cmath>
#include <sstream>

//==============================================================================
// Global profiler instance and thread-local storage
//==============================================================================

static std::unique_ptr<PerformanceProfiler> globalProfiler;
static std::mutex globalProfilerMutex;

// Thread-local storage for RT-safe timer tracking
thread_local std::unordered_map<std::string, PerformanceProfiler::ActiveTimer> PerformanceProfiler::tlsActiveTimers;

PerformanceProfiler& getGlobalProfiler()
{
    std::lock_guard<std::mutex> lock(globalProfilerMutex);
    if (!globalProfiler)
        globalProfiler = std::make_unique<PerformanceProfiler>();
    return *globalProfiler;
}

//==============================================================================
// Constructor & Destructor
//==============================================================================

PerformanceProfiler::PerformanceProfiler()
{
    // Initialize pipeline latency tracking
    for (auto& latency : pipelineLatencies)
    {
        latency.store(0.0, std::memory_order_relaxed);
    }
    
    lastAlertCheck = std::chrono::system_clock::now();
}

PerformanceProfiler::~PerformanceProfiler() = default;

//==============================================================================
// ScopedTimer Implementation
//==============================================================================

PerformanceProfiler::ScopedTimer::ScopedTimer(PerformanceProfiler& profiler, const std::string& name)
    : profiler(profiler), timerName(name)
{
    startTime = std::chrono::high_resolution_clock::now();
}

PerformanceProfiler::ScopedTimer::~ScopedTimer()
{
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    profiler.recordTiming(timerName, static_cast<double>(duration.count()));
}

PerformanceProfiler::ScopedTimer PerformanceProfiler::createScopedTimer(const std::string& name)
{
    return ScopedTimer(*this, name);
}

//==============================================================================
// Core Profiling Interface
//==============================================================================

void PerformanceProfiler::startTimer(const std::string& name)
{
    if (!profilingEnabled.load()) return;
    
    // RT-SAFE: Use thread-local storage for timer tracking
    tlsActiveTimers[name] = {std::chrono::high_resolution_clock::now(), name, true};
}

void PerformanceProfiler::endTimer(const std::string& name)
{
    if (!profilingEnabled.load()) return;
    
    auto endTime = std::chrono::high_resolution_clock::now();
    
    // RT-SAFE: Use thread-local storage without locks
    auto it = tlsActiveTimers.find(name);
    if (it != tlsActiveTimers.end() && it->second.active)
    {
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - it->second.startTime);
        it->second.active = false; // Mark as completed
        recordTiming(name, static_cast<double>(duration.count()));
    }
}

void PerformanceProfiler::recordTiming(const std::string& name, double microseconds)
{
    if (!profilingEnabled.load()) return;
    
    addTimingSample(name, microseconds);
}

//==============================================================================
// Pipeline Monitoring
//==============================================================================

void PerformanceProfiler::startPipelineStage(PipelineStage stage)
{
    if (!profilingEnabled.load()) return;
    
    std::string stageName = pipelineStageToString(stage);
    startTimer(stageName);
}

void PerformanceProfiler::endPipelineStage(PipelineStage stage)
{
    if (!profilingEnabled.load()) return;
    
    std::string stageName = pipelineStageToString(stage);
    endTimer(stageName);
}

void PerformanceProfiler::recordPipelineExecution(const PipelineExecution& execution)
{
    if (!profilingEnabled.load()) return;
    
    recordTiming("Pipeline_PaintCapture", execution.paintCapture_us);
    recordTiming("Pipeline_SpatialGridLookup", execution.spatialGridLookup_us);
    recordTiming("Pipeline_ParameterMapping", execution.parameterMapping_us);
    recordTiming("Pipeline_SampleSelection", execution.sampleSelection_us);
    recordTiming("Pipeline_AudioProcessing", execution.audioProcessing_us);
    recordTiming("Pipeline_BufferOutput", execution.bufferOutput_us);
    recordTiming("Pipeline_TotalLatency", execution.totalLatency_us);
    
    // Check for performance alerts
    if (!execution.meetsLatencyTarget(latencyTarget.load()))
    {
        addAlert(PerformanceAlert::Severity::Warning, 
                "Pipeline execution exceeded latency target: " + 
                std::to_string(execution.totalLatency_us) + "µs", "latency");
    }
}

//==============================================================================
// Performance Statistics
//==============================================================================

PerformanceProfiler::TimingStats PerformanceProfiler::getTimingStats(const std::string& name) const
{
    TimingStats stats;
    stats.name = name;
    
    // NON_RT: This method should only be called from non-realtime threads
    std::lock_guard<std::mutex> lock(timingDataMutex);
    auto it = timingData.find(name);
    if (it != timingData.end() && it->second)
    {
        const auto& samples = it->second->samples;
        if (!samples.empty())
        {
            stats.updateFromSamples(samples);
            stats.targetTime_us = latencyTarget.load();
            stats.exceedsTarget = stats.averageTime_us > stats.targetTime_us;
        }
    }
    
    return stats;
}

std::vector<PerformanceProfiler::TimingStats> PerformanceProfiler::getAllTimingStats() const
{
    std::vector<TimingStats> allStats;
    
    // NON_RT: This method should only be called from non-realtime threads
    std::lock_guard<std::mutex> lock(timingDataMutex);
    for (const auto& pair : timingData)
    {
        if (pair.second && !pair.second->samples.empty())
        {
            allStats.push_back(getTimingStats(pair.first));
        }
    }
    
    return allStats;
}

PerformanceProfiler::TimingStats PerformanceProfiler::getPipelineStats(PipelineStage stage) const
{
    std::string stageName = pipelineStageToString(stage);
    return getTimingStats("Pipeline_" + stageName);
}

std::vector<PerformanceProfiler::TimingStats> PerformanceProfiler::getAllPipelineStats() const
{
    std::vector<TimingStats> pipelineStats;
    
    for (int i = 0; i < 7; ++i) // 7 pipeline stages
    {
        auto stage = static_cast<PipelineStage>(i);
        auto stats = getPipelineStats(stage);
        if (stats.sampleCount > 0)
        {
            pipelineStats.push_back(stats);
        }
    }
    
    return pipelineStats;
}

void PerformanceProfiler::TimingStats::updateFromSamples(const std::vector<double>& samples)
{
    if (samples.empty()) return;
    
    sampleCount = static_cast<uint64_t>(samples.size());
    
    // Calculate basic statistics
    totalTime_us = 0.0;
    minTime_us = std::numeric_limits<double>::max();
    maxTime_us = 0.0;
    
    for (double sample : samples)
    {
        totalTime_us += sample;
        minTime_us = std::min(minTime_us, sample);
        maxTime_us = std::max(maxTime_us, sample);
        
        if (sample > targetTime_us)
            exceedCount++;
    }
    
    averageTime_us = totalTime_us / static_cast<double>(sampleCount);
    
    // Calculate advanced statistics
    std::vector<double> sortedSamples = samples;
    std::sort(sortedSamples.begin(), sortedSamples.end());
    
    // Median
    if (sortedSamples.size() % 2 == 0)
    {
        median_us = (sortedSamples[sortedSamples.size() / 2 - 1] + 
                    sortedSamples[sortedSamples.size() / 2]) / 2.0;
    }
    else
    {
        median_us = sortedSamples[sortedSamples.size() / 2];
    }
    
    // Percentiles
    size_t p95_index = static_cast<size_t>(sortedSamples.size() * 0.95);
    size_t p99_index = static_cast<size_t>(sortedSamples.size() * 0.99);
    
    percentile95_us = sortedSamples[std::min(p95_index, sortedSamples.size() - 1)];
    percentile99_us = sortedSamples[std::min(p99_index, sortedSamples.size() - 1)];
    
    // Standard deviation
    double variance = 0.0;
    for (double sample : samples)
    {
        double diff = sample - averageTime_us;
        variance += diff * diff;
    }
    variance /= static_cast<double>(sampleCount);
    standardDeviation_us = std::sqrt(variance);
}

//==============================================================================
// Real-Time Monitoring
//==============================================================================

PerformanceProfiler::PerformanceSnapshot PerformanceProfiler::getCurrentSnapshot() const
{
    PerformanceSnapshot snapshot;
    snapshot.timestamp = std::chrono::system_clock::now();
    
    // Get recent performance metrics
    auto totalLatencyStats = getTimingStats("Pipeline_TotalLatency");
    if (totalLatencyStats.sampleCount > 0)
    {
        snapshot.recentAverageLatency_us = totalLatencyStats.averageTime_us;
        snapshot.recentMaxLatency_us = totalLatencyStats.maxTime_us;
    }
    
    // CPU and memory usage (simplified - would need platform-specific code for real implementation)
    snapshot.currentCpuUsage = 0.0; // TODO: Implement platform-specific CPU monitoring
    snapshot.currentMemoryUsage = 0.0; // TODO: Implement memory monitoring
    
    // Count recent dropouts (samples that exceeded latency target)
    snapshot.recentDropouts = static_cast<uint32_t>(totalLatencyStats.exceedCount);
    
    return snapshot;
}

std::vector<PerformanceProfiler::PerformanceAlert> PerformanceProfiler::getRecentAlerts() const
{
    std::lock_guard<std::mutex> lock(alertsMutex);
    return recentAlerts;
}

void PerformanceProfiler::clearAlerts()
{
    std::lock_guard<std::mutex> lock(alertsMutex);
    recentAlerts.clear();
}

//==============================================================================
// Configuration & Control
//==============================================================================

void PerformanceProfiler::reset()
{
    // NON_RT: This method should only be called from non-realtime threads
    std::lock_guard<std::mutex> lock(timingDataMutex);
    timingData.clear();
    
    // Reset atomic values
    recentLatency.store(0.0);
    recentMaxLatency.store(0.0);
    recentSampleCount.store(0);
    latencyTargetExceeded.store(false);
    
    // Reset pipeline latencies
    for (auto& latency : pipelineLatencies)
    {
        latency.store(0.0);
    }
    
    // Reset alert counters
    criticalAlertCount.store(0);
    warningAlertCount.store(0);
    infoAlertCount.store(0);
    
    clearAlerts();
}

void PerformanceProfiler::resetPipelineStats()
{
    // NON_RT: This method should only be called from non-realtime threads
    std::lock_guard<std::mutex> lock(timingDataMutex);
    
    // Reset pipeline latencies atomically
    for (auto& latency : pipelineLatencies)
    {
        latency.store(0.0);
    }
    
    // Clear pipeline timing data from main storage
    std::vector<std::string> pipelineKeys;
    for (const auto& pair : timingData)
    {
        if (pair.first.find("Pipeline_") == 0)
        {
            pipelineKeys.push_back(pair.first);
        }
    }
    
    for (const auto& key : pipelineKeys)
    {
        timingData.erase(key);
    }
}

//==============================================================================
// Export & Reporting
//==============================================================================

PerformanceProfiler::PerformanceReport PerformanceProfiler::generateReport() const
{
    PerformanceReport report;
    report.reportTime = std::chrono::system_clock::now();
    report.timingStats = getAllTimingStats();
    report.pipelineStats = getAllPipelineStats();
    report.alerts = getRecentAlerts();
    report.currentSnapshot = getCurrentSnapshot();
    
    // Calculate overall health score
    report.overallHealthScore = calculateHealthScore();
    report.meetsPerformanceRequirements = report.overallHealthScore > 0.8;
    
    // Generate recommendations
    std::ostringstream recommendations;
    if (report.overallHealthScore < 0.5)
    {
        recommendations << "CRITICAL: System performance is severely degraded. ";
    }
    else if (report.overallHealthScore < 0.8)
    {
        recommendations << "WARNING: System performance needs attention. ";
    }
    
    // Specific recommendations based on pipeline stats
    for (const auto& stats : report.pipelineStats)
    {
        if (stats.averageTime_us > stats.targetTime_us)
        {
            recommendations << "Optimize " << stats.name << " (avg: " 
                           << std::fixed << std::setprecision(1) << stats.averageTime_us 
                           << "µs). ";
        }
    }
    
    report.recommendations = recommendations.str();
    
    return report;
}

std::string PerformanceProfiler::generateTextReport() const
{
    auto report = generateReport();
    std::ostringstream oss;
    
    oss << "=== SpectralCanvas Pro Performance Report ===\n";
    oss << "Generated: " << std::chrono::duration_cast<std::chrono::seconds>(
              report.reportTime.time_since_epoch()).count() << "\n";
    oss << "Overall Health Score: " << std::fixed << std::setprecision(2) 
        << report.overallHealthScore << "/1.0\n";
    oss << "Meets Requirements: " << (report.meetsPerformanceRequirements ? "YES" : "NO") << "\n\n";
    
    // Pipeline performance
    oss << "=== Pipeline Performance ===\n";
    for (const auto& stats : report.pipelineStats)
    {
        oss << stats.name << ":\n";
        oss << "  Average: " << std::fixed << std::setprecision(1) << stats.averageTime_us << "µs\n";
        oss << "  95th Percentile: " << stats.percentile95_us << "µs\n";
        oss << "  Max: " << stats.maxTime_us << "µs\n";
        oss << "  Samples: " << stats.sampleCount << "\n";
        oss << "  Exceeds Target: " << (stats.exceedsTarget ? "YES" : "NO") << "\n\n";
    }
    
    // Current snapshot
    oss << "=== Current Status ===\n";
    oss << "Recent Average Latency: " << std::fixed << std::setprecision(1) 
        << report.currentSnapshot.recentAverageLatency_us << "µs\n";
    oss << "Recent Max Latency: " << report.currentSnapshot.recentMaxLatency_us << "µs\n";
    oss << "Recent Dropouts: " << report.currentSnapshot.recentDropouts << "\n";
    oss << "System Healthy: " << (report.currentSnapshot.isHealthy() ? "YES" : "NO") << "\n\n";
    
    // Recommendations
    if (!report.recommendations.empty())
    {
        oss << "=== Recommendations ===\n";
        oss << report.recommendations << "\n\n";
    }
    
    // Recent alerts
    if (!report.alerts.empty())
    {
        oss << "=== Recent Alerts ===\n";
        for (const auto& alert : report.alerts)
        {
            oss << "[" << (alert.severity == PerformanceAlert::Severity::Critical ? "CRITICAL" :
                          alert.severity == PerformanceAlert::Severity::Warning ? "WARNING" : "INFO")
                << "] " << alert.message << "\n";
        }
    }
    
    return oss.str();
}

bool PerformanceProfiler::exportReportToFile(const juce::File& outputFile) const
{
    auto textReport = generateTextReport();
    return outputFile.replaceWithText(textReport);
}

//==============================================================================
// Private Implementation
//==============================================================================

void PerformanceProfiler::TimingData::addSample(double microseconds)
{
    samples.push_back(microseconds);
    totalSamples.fetch_add(1);
    lastUpdate = std::chrono::high_resolution_clock::now();
}

void PerformanceProfiler::TimingData::trimToMaxSize(uint32_t maxSize)
{
    if (samples.size() > maxSize)
    {
        // Keep the most recent samples
        samples.erase(samples.begin(), samples.begin() + (samples.size() - maxSize));
    }
}

std::string PerformanceProfiler::pipelineStageToString(PipelineStage stage) const
{
    switch (stage)
    {
        case PipelineStage::PaintCapture: return "PaintCapture";
        case PipelineStage::SpatialGridLookup: return "SpatialGridLookup";
        case PipelineStage::ParameterMapping: return "ParameterMapping";
        case PipelineStage::SampleSelection: return "SampleSelection";
        case PipelineStage::AudioProcessing: return "AudioProcessing";
        case PipelineStage::BufferOutput: return "BufferOutput";
        case PipelineStage::TotalLatency: return "TotalLatency";
        default: return "Unknown";
    }
}

PerformanceProfiler::TimingData* PerformanceProfiler::getOrCreateTimingData(const std::string& name)
{
    auto it = timingData.find(name);
    if (it == timingData.end())
    {
        it = timingData.emplace(name, std::make_unique<TimingData>()).first;
    }
    return it->second.get();
}

void PerformanceProfiler::addTimingSample(const std::string& name, double microseconds)
{
    // RT-SAFE: Update atomic values directly for real-time access
    recentLatency.store(microseconds);
    
    // Update max if this is larger
    double currentMax = recentMaxLatency.load();
    while (microseconds > currentMax && 
           !recentMaxLatency.compare_exchange_weak(currentMax, microseconds))
    {
        // CAS loop to update max atomically
    }
    
    recentSampleCount.fetch_add(1);
    
    // Check target exceeded for fast RT access
    bool exceedsTarget = microseconds > latencyTarget.load();
    latencyTargetExceeded.store(exceedsTarget);
    
    // RT-SAFE: Update pipeline latencies if this is a pipeline measurement
    if (name.find("Pipeline_") == 0)
    {
        // Map pipeline names to indices for atomic array access
        if (name == "Pipeline_PaintCapture") pipelineLatencies[0].store(microseconds);
        else if (name == "Pipeline_SpatialGridLookup") pipelineLatencies[1].store(microseconds);
        else if (name == "Pipeline_ParameterMapping") pipelineLatencies[2].store(microseconds);
        else if (name == "Pipeline_SampleSelection") pipelineLatencies[3].store(microseconds);
        else if (name == "Pipeline_AudioProcessing") pipelineLatencies[4].store(microseconds);
        else if (name == "Pipeline_BufferOutput") pipelineLatencies[5].store(microseconds);
        else if (name == "Pipeline_TotalLatency") pipelineLatencies[6].store(microseconds);
    }
    
    // RT-SAFE: Increment alert counters without string operations
    if (exceedsTarget)
    {
        if (microseconds > latencyTarget.load() * 2.0)
            criticalAlertCount.fetch_add(1);
        else
            warningAlertCount.fetch_add(1);
    }
    
    // NON_RT: Only update full timing data in background thread
    // This would be called from a separate low-priority thread
    // updateDetailedTimingData(name, microseconds);
}

void PerformanceProfiler::addAlert(PerformanceAlert::Severity severity, const std::string& message, const std::string& category)
{
    // NON_RT: This method should only be called from non-realtime threads
    std::lock_guard<std::mutex> lock(alertsMutex);
    
    PerformanceAlert alert;
    alert.severity = severity;
    alert.message = message;
    alert.timestamp = std::chrono::system_clock::now();
    alert.category = category;
    
    recentAlerts.push_back(alert);
    
    // Keep only recent alerts (last 100)
    if (recentAlerts.size() > 100)
    {
        recentAlerts.erase(recentAlerts.begin());
    }
}

double PerformanceProfiler::calculateHealthScore() const
{
    // Simple health score calculation based on latency performance
    auto totalLatencyStats = getTimingStats("Pipeline_TotalLatency");
    
    if (totalLatencyStats.sampleCount == 0)
        return 1.0; // No data = assume healthy
    
    double targetLatency = latencyTarget.load();
    double averageLatency = totalLatencyStats.averageTime_us;
    
    // Score based on how well we meet the latency target
    if (averageLatency <= targetLatency)
    {
        return 1.0; // Perfect score
    }
    else
    {
        // Linear decay as latency increases beyond target
        double overage = averageLatency - targetLatency;
        double maxOverage = targetLatency; // 100% overage = 0 score
        return std::max(0.0, 1.0 - (overage / maxOverage));
    }
}