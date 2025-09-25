/******************************************************************************
 * File: QualityMonitor.cpp
 * Description: Implementation of permanent quality monitoring system
 * 
 * Code Quality Guardian - Continuous system health monitoring
 * 
 * Copyright (c) 2025 Spectral Audio Systems
 ******************************************************************************/

#include "QualityMonitor.h"
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <cmath>

//==============================================================================
// QualityMonitor Implementation
//==============================================================================

QualityMonitor::QualityMonitor()
{
    // Reserve space for performance history
    cpuHistory.reserve(MAX_HISTORY_SIZE);
    memoryHistory.reserve(MAX_HISTORY_SIZE);
    eventHistory.reserve(MAX_HISTORY_SIZE);
    
    DBG("🛡️ QualityMonitor: System initialized");
}

QualityMonitor::~QualityMonitor()
{
    stopMonitoring();
    DBG("🛡️ QualityMonitor: System shutdown");
}

void QualityMonitor::startMonitoring()
{
    if (monitoringActive.load())
        return;
    
    monitoringActive.store(true);
    monitoringStartTime = std::chrono::steady_clock::now();
    
    // Start monitoring timer (100ms intervals)
    startTimer(100);
    
    // Log startup event
    QualityEventData startupEvent;
    startupEvent.event = QualityEvent::SystemStartup;
    startupEvent.timestamp = std::chrono::steady_clock::now();
    startupEvent.component = "QualityMonitor";
    startupEvent.message = "Quality monitoring system started";
    startupEvent.severity = 0.0f;
    
    logQualityEvent(startupEvent);
    
    DBG("🛡️ QualityMonitor: Monitoring started");
}

void QualityMonitor::stopMonitoring()
{
    if (!monitoringActive.load())
        return;
    
    monitoringActive.store(false);
    stopTimer();
    
    // Log shutdown event
    QualityEventData shutdownEvent;
    shutdownEvent.event = QualityEvent::SystemShutdown;
    shutdownEvent.timestamp = std::chrono::steady_clock::now();
    shutdownEvent.component = "QualityMonitor";
    shutdownEvent.message = "Quality monitoring system stopped";
    shutdownEvent.severity = 0.0f;
    
    logQualityEvent(shutdownEvent);
    
    DBG("🛡️ QualityMonitor: Monitoring stopped");
}

void QualityMonitor::timerCallback()
{
    if (!monitoringActive.load())
        return;
    
    // Update system health
    updateSystemHealth();
    
    // Check performance thresholds
    checkPerformanceThresholds();
    
    // Calculate overall health score
    calculateHealthScore();
    
    // Analyze performance trends
    analyzeCpuTrends();
    analyzeMemoryTrends();
    
    // Clean up old events
    cleanupOldEvents();
}

void QualityMonitor::updateCpuUsage(float usage)
{
    metrics.cpuUsagePercent.store(usage);
    
    // Track peak usage
    float currentPeak = metrics.peakCpuUsage.load();
    if (usage > currentPeak)
    {
        metrics.peakCpuUsage.store(usage);
    }
    
    // Count CPU spikes
    if (usage > 80.0f)
    {
        metrics.cpuSpikes.fetch_add(1);
    }
    
    // Add to history for trend analysis
    juce::ScopedLock lock(metricsLock);
    cpuHistory.push_back(usage);
    if (cpuHistory.size() > MAX_HISTORY_SIZE)
    {
        cpuHistory.erase(cpuHistory.begin());
    }
}

void QualityMonitor::updateMemoryUsage(size_t usageMB)
{
    metrics.memoryUsageMB.store(usageMB);
    
    // Track peak usage
    size_t currentPeak = metrics.peakMemoryUsage.load();
    if (usageMB > currentPeak)
    {
        metrics.peakMemoryUsage.store(usageMB);
    }
    
    // Add to history for trend analysis
    juce::ScopedLock lock(metricsLock);
    memoryHistory.push_back(usageMB);
    if (memoryHistory.size() > MAX_HISTORY_SIZE)
    {
        memoryHistory.erase(memoryHistory.begin());
    }
}

void QualityMonitor::updateAudioLatency(float latencyMs)
{
    metrics.audioLatencyMs.store(latencyMs);
    
    // Check for acceptable latency
    if (latencyMs > audioLatencyThreshold)
    {
        triggerAlert("audio_latency", 
                    "Audio latency exceeded threshold: " + std::to_string(latencyMs) + "ms");
    }
    else
    {
        clearAlert("audio_latency");
    }
}

void QualityMonitor::updatePaintLatency(float latencyMs)
{
    metrics.paintToAudioLatencyMs.store(latencyMs);
    
    // Check for acceptable paint responsiveness
    if (latencyMs > paintLatencyThreshold)
    {
        triggerAlert("paint_latency", 
                    "Paint-to-audio latency exceeded threshold: " + std::to_string(latencyMs) + "ms");
    }
    else
    {
        clearAlert("paint_latency");
    }
}

void QualityMonitor::reportAudioDropout()
{
    metrics.audioDropouts.fetch_add(1);
    
    QualityEventData dropoutEvent;
    dropoutEvent.event = QualityEvent::AudioAlert;
    dropoutEvent.timestamp = std::chrono::steady_clock::now();
    dropoutEvent.component = "AudioSystem";
    dropoutEvent.message = "Audio dropout detected";
    dropoutEvent.severity = 0.7f;
    
    logQualityEvent(dropoutEvent);
    
    triggerAlert("audio_dropouts", "Audio dropouts detected: " + 
                 std::to_string(metrics.audioDropouts.load()));
}

void QualityMonitor::reportPaintEventMissed()
{
    metrics.paintEventsMissed.fetch_add(1);
    
    QualityEventData missedEvent;
    missedEvent.event = QualityEvent::PerformanceAlert;
    missedEvent.timestamp = std::chrono::steady_clock::now();
    missedEvent.component = "PaintSystem";
    missedEvent.message = "Paint event missed";
    missedEvent.severity = 0.5f;
    
    logQualityEvent(missedEvent);
}

void QualityMonitor::reportComponentHealth(const std::string& component, bool healthy)
{
    // Update specific component health flags
    if (component == "SpectralSynthEngine")
    {
        metrics.spectralEngineHealthy.store(healthy);
    }
    else if (component == "AudioDevice")
    {
        metrics.audioDeviceHealthy.store(healthy);
    }
    else if (component == "PaintSystem")
    {
        metrics.paintSystemHealthy.store(healthy);
    }
    
    if (!healthy)
    {
        triggerAlert("component_" + component, component + " is unhealthy");
    }
    else
    {
        clearAlert("component_" + component);
    }
}

void QualityMonitor::reportComponentInitialized(const std::string& component)
{
    QualityEventData initEvent;
    initEvent.event = QualityEvent::ComponentInitialized;
    initEvent.timestamp = std::chrono::steady_clock::now();
    initEvent.component = component;
    initEvent.message = component + " initialized successfully";
    initEvent.severity = 0.0f;
    
    logQualityEvent(initEvent);
    
    DBG("🛡️ QualityMonitor: " << component << " initialized");
}

void QualityMonitor::reportComponentFailed(const std::string& component, const std::string& error)
{
    QualityEventData failEvent;
    failEvent.event = QualityEvent::ComponentFailed;
    failEvent.timestamp = std::chrono::steady_clock::now();
    failEvent.component = component;
    failEvent.message = component + " failed: " + error;
    failEvent.severity = 0.9f;
    
    logQualityEvent(failEvent);
    
    metrics.criticalErrorsCount.fetch_add(1);
    
    triggerAlert("component_failure_" + component, 
                 component + " failed: " + error);
    
    DBG("🚨 QualityMonitor: " << component << " FAILED - " << error);
}

void QualityMonitor::reportRecoveryEvent(const std::string& component, const std::string& action)
{
    QualityEventData recoveryEvent;
    recoveryEvent.event = QualityEvent::RecoveryTriggered;
    recoveryEvent.timestamp = std::chrono::steady_clock::now();
    recoveryEvent.component = component;
    recoveryEvent.message = component + " recovery: " + action;
    recoveryEvent.severity = 0.3f;
    
    logQualityEvent(recoveryEvent);
    
    metrics.recoveryEventsCount.fetch_add(1);
    
    DBG("🔧 QualityMonitor: " << component << " recovery - " << action);
}

float QualityMonitor::calculateOverallHealthScore() const
{
    float score = 100.0f;
    
    // Deduct points for performance issues
    float cpuUsage = metrics.cpuUsagePercent.load();
    if (cpuUsage > 80.0f)
        score -= (cpuUsage - 80.0f) * 2.0f; // -2 points per % over 80%
    
    size_t memoryUsage = metrics.memoryUsageMB.load();
    if (memoryUsage > memoryThreshold)
        score -= std::min(20.0f, (float)(memoryUsage - memoryThreshold) * 0.1f);
    
    // Deduct points for audio issues
    int audioDropouts = metrics.audioDropouts.load();
    score -= std::min(30.0f, audioDropouts * 5.0f);
    
    // Deduct points for component health
    if (!metrics.spectralEngineHealthy.load())
        score -= 25.0f;
    if (!metrics.audioDeviceHealthy.load())
        score -= 25.0f;
    if (!metrics.paintSystemHealthy.load())
        score -= 15.0f;
    
    // Deduct points for critical errors
    int criticalErrors = metrics.criticalErrorsCount.load();
    score -= std::min(40.0f, criticalErrors * 10.0f);
    
    return std::max(0.0f, score);
}

bool QualityMonitor::isSystemHealthy() const
{
    return calculateOverallHealthScore() >= 70.0f;
}

std::vector<std::string> QualityMonitor::getActiveAlerts() const
{
    juce::ScopedLock lock(alertLock);
    
    std::vector<std::string> alerts;
    for (const auto& alert : activeAlerts)
    {
        alerts.push_back(alert.second);
    }
    
    return alerts;
}

std::string QualityMonitor::generateHealthReport() const
{
    std::ostringstream report;
    
    auto now = std::chrono::steady_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - monitoringStartTime);
    
    report << "🛡️ SPECTRAL CANVAS PRO - QUALITY HEALTH REPORT\n";
    report << "================================================\n";
    report << "Monitoring Uptime: " << uptime.count() << " seconds\n";
    report << "Overall Health Score: " << std::fixed << std::setprecision(1) 
           << calculateOverallHealthScore() << "%\n\n";
    
    report << "📊 PERFORMANCE METRICS:\n";
    report << "  CPU Usage: " << std::fixed << std::setprecision(1) 
           << metrics.cpuUsagePercent.load() << "% (Peak: " 
           << metrics.peakCpuUsage.load() << "%)\n";
    report << "  Memory Usage: " << metrics.memoryUsageMB.load() 
           << "MB (Peak: " << metrics.peakMemoryUsage.load() << "MB)\n";
    report << "  Audio Latency: " << std::fixed << std::setprecision(2) 
           << metrics.audioLatencyMs.load() << "ms\n";
    report << "  Paint-to-Audio Latency: " << std::fixed << std::setprecision(2) 
           << metrics.paintToAudioLatencyMs.load() << "ms\n\n";
    
    report << "🚨 ISSUES DETECTED:\n";
    report << "  CPU Spikes: " << metrics.cpuSpikes.load() << "\n";
    report << "  Audio Dropouts: " << metrics.audioDropouts.load() << "\n";
    report << "  Paint Events Missed: " << metrics.paintEventsMissed.load() << "\n";
    report << "  Critical Errors: " << metrics.criticalErrorsCount.load() << "\n";
    report << "  Recovery Events: " << metrics.recoveryEventsCount.load() << "\n\n";
    
    report << "💚 COMPONENT HEALTH:\n";
    report << "  SpectralSynthEngine: " << (metrics.spectralEngineHealthy.load() ? "✅ Healthy" : "❌ Failed") << "\n";
    report << "  Audio Device: " << (metrics.audioDeviceHealthy.load() ? "✅ Healthy" : "❌ Failed") << "\n";
    report << "  Paint System: " << (metrics.paintSystemHealthy.load() ? "✅ Healthy" : "❌ Failed") << "\n\n";
    
    auto alerts = getActiveAlerts();
    if (!alerts.empty())
    {
        report << "⚠️ ACTIVE ALERTS:\n";
        for (const auto& alert : alerts)
        {
            report << "  • " << alert << "\n";
        }
    }
    else
    {
        report << "✅ NO ACTIVE ALERTS\n";
    }
    
    return report.str();
}

void QualityMonitor::setPerformanceThresholds(float maxCpuPercent, size_t maxMemoryMB, 
                                             float maxAudioLatencyMs, float maxPaintLatencyMs)
{
    cpuThreshold = maxCpuPercent;
    memoryThreshold = maxMemoryMB;
    audioLatencyThreshold = maxAudioLatencyMs;
    paintLatencyThreshold = maxPaintLatencyMs;
    
    DBG("🛡️ QualityMonitor: Performance thresholds updated - CPU: " << maxCpuPercent 
        << "%, Memory: " << maxMemoryMB << "MB, Audio: " << maxAudioLatencyMs 
        << "ms, Paint: " << maxPaintLatencyMs << "ms");
}

void QualityMonitor::addEventListener(std::function<void(const QualityEventData&)> listener)
{
    juce::ScopedLock lock(eventLock);
    eventListeners.push_back(listener);
}

void QualityMonitor::removeAllEventListeners()
{
    juce::ScopedLock lock(eventLock);
    eventListeners.clear();
}

void QualityMonitor::runDiagnosticTests()
{
    DBG("🔍 QualityMonitor: Running diagnostic tests...");
    
    // Test performance measurement
    {
        auto start = std::chrono::high_resolution_clock::now();
        juce::Thread::sleep(10); // Simulate 10ms operation
        auto end = std::chrono::high_resolution_clock::now();
        
        float duration = std::chrono::duration<float, std::milli>(end - start).count();
        DBG("🔍 Performance measurement test: " << duration << "ms");
    }
    
    // Test component reporting
    reportComponentInitialized("DiagnosticTest");
    reportComponentHealth("DiagnosticTest", true);
    
    // Test alert system
    triggerAlert("diagnostic_test", "This is a test alert");
    clearAlert("diagnostic_test");
    
    DBG("🔍 QualityMonitor: Diagnostic tests completed");
}

void QualityMonitor::simulateStressTest(int durationSeconds)
{
    DBG("💪 QualityMonitor: Starting stress test for " << durationSeconds << " seconds");
    
    // Simulate high CPU usage
    for (int i = 0; i < durationSeconds * 10; ++i)
    {
        updateCpuUsage(85.0f + (i % 10)); // Fluctuating high CPU
        updateMemoryUsage(300 + (i % 50)); // Fluctuating memory usage
        juce::Thread::sleep(100);
    }
    
    DBG("💪 QualityMonitor: Stress test completed");
}

void QualityMonitor::exportMetricsToFile(const juce::File& file) const
{
    std::ostringstream csv;
    
    csv << "Timestamp,CPU_Usage,Memory_Usage,Audio_Latency,Paint_Latency,Health_Score\n";
    
    // Export current metrics (in a real implementation, we'd store historical data)
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    csv << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << ","
        << metrics.cpuUsagePercent.load() << ","
        << metrics.memoryUsageMB.load() << ","
        << metrics.audioLatencyMs.load() << ","
        << metrics.paintToAudioLatencyMs.load() << ","
        << calculateOverallHealthScore() << "\n";
    
    file.replaceWithText(csv.str());
    DBG("📊 QualityMonitor: Metrics exported to " << file.getFullPathName());
}

// Private Methods

void QualityMonitor::updateSystemHealth()
{
    // Update component health based on recent events and metrics
    bool engineHealthy = (metrics.criticalErrorsCount.load() == 0) && 
                        (metrics.cpuUsagePercent.load() < 90.0f);
    
    bool audioHealthy = (metrics.audioDropouts.load() < 5) && 
                       (metrics.audioLatencyMs.load() < audioLatencyThreshold * 2);
    
    bool paintHealthy = (metrics.paintEventsMissed.load() < 10) && 
                       (metrics.paintToAudioLatencyMs.load() < paintLatencyThreshold * 2);
    
    metrics.spectralEngineHealthy.store(engineHealthy);
    metrics.audioDeviceHealthy.store(audioHealthy);
    metrics.paintSystemHealthy.store(paintHealthy);
}

void QualityMonitor::checkPerformanceThresholds()
{
    float cpuUsage = metrics.cpuUsagePercent.load();
    size_t memoryUsage = metrics.memoryUsageMB.load();
    
    if (cpuUsage > cpuThreshold)
    {
        triggerAlert("cpu_threshold", 
                    "CPU usage exceeded threshold: " + std::to_string(cpuUsage) + "%");
    }
    else
    {
        clearAlert("cpu_threshold");
    }
    
    if (memoryUsage > memoryThreshold)
    {
        triggerAlert("memory_threshold", 
                    "Memory usage exceeded threshold: " + std::to_string(memoryUsage) + "MB");
    }
    else
    {
        clearAlert("memory_threshold");
    }
}

void QualityMonitor::logQualityEvent(const QualityEventData& event)
{
    juce::ScopedLock lock(eventLock);
    
    eventHistory.push_back(event);
    
    // Limit event history size
    if (eventHistory.size() > MAX_HISTORY_SIZE)
    {
        eventHistory.erase(eventHistory.begin());
    }
    
    // Notify listeners
    for (auto& listener : eventListeners)
    {
        listener(event);
    }
}

void QualityMonitor::calculateHealthScore()
{
    float score = calculateOverallHealthScore();
    metrics.overallHealthScore.store(score);
    
    // Trigger quality degradation event if score drops significantly
    static float lastScore = 100.0f;
    if (score < lastScore - 10.0f)
    {
        QualityEventData degradationEvent;
        degradationEvent.event = QualityEvent::QualityDegraded;
        degradationEvent.timestamp = std::chrono::steady_clock::now();
        degradationEvent.component = "System";
        degradationEvent.message = "Quality score degraded from " + 
                                  std::to_string(lastScore) + "% to " + std::to_string(score) + "%";
        degradationEvent.severity = 0.6f;
        
        logQualityEvent(degradationEvent);
    }
    
    lastScore = score;
}

void QualityMonitor::cleanupOldEvents()
{
    juce::ScopedLock lock(eventLock);
    
    auto now = std::chrono::steady_clock::now();
    auto cutoff = now - std::chrono::hours(24); // Keep 24 hours of events
    
    eventHistory.erase(
        std::remove_if(eventHistory.begin(), eventHistory.end(),
                      [cutoff](const QualityEventData& event) {
                          return event.timestamp < cutoff;
                      }),
        eventHistory.end());
}

void QualityMonitor::analyzeCpuTrends()
{
    juce::ScopedLock lock(metricsLock);
    
    if (cpuHistory.size() < 10)
        return;
    
    // Calculate trend over last 10 samples
    float sum = 0.0f;
    for (size_t i = cpuHistory.size() - 10; i < cpuHistory.size(); ++i)
    {
        sum += cpuHistory[i];
    }
    
    float average = sum / 10.0f;
    if (average > cpuThreshold * 0.8f) // 80% of threshold
    {
        triggerAlert("cpu_trend", "CPU usage trending high: " + std::to_string(average) + "%");
    }
}

void QualityMonitor::analyzeMemoryTrends()
{
    juce::ScopedLock lock(metricsLock);
    
    if (memoryHistory.size() < 10)
        return;
    
    // Check for memory growth trend
    size_t recent = 0, older = 0;
    for (size_t i = memoryHistory.size() - 5; i < memoryHistory.size(); ++i)
        recent += memoryHistory[i];
    for (size_t i = memoryHistory.size() - 10; i < memoryHistory.size() - 5; ++i)
        older += memoryHistory[i];
    
    float recentAvg = recent / 5.0f;
    float olderAvg = older / 5.0f;
    
    if (recentAvg > olderAvg * 1.1f) // 10% growth
    {
        triggerAlert("memory_growth", "Memory usage growing: " + 
                    std::to_string(recentAvg) + "MB (was " + std::to_string(olderAvg) + "MB)");
    }
}

void QualityMonitor::detectPerformanceRegression()
{
    // Implementation for performance regression detection
    // Compare current metrics against baseline
}

void QualityMonitor::triggerAlert(const std::string& alertId, const std::string& message)
{
    juce::ScopedLock lock(alertLock);
    
    if (activeAlerts.find(alertId) == activeAlerts.end())
    {
        activeAlerts[alertId] = message;
        DBG("⚠️ QualityMonitor Alert: " << message);
    }
}

void QualityMonitor::clearAlert(const std::string& alertId)
{
    juce::ScopedLock lock(alertLock);
    
    auto it = activeAlerts.find(alertId);
    if (it != activeAlerts.end())
    {
        activeAlerts.erase(it);
    }
}

//==============================================================================
// Global Quality Monitor Access
//==============================================================================

QualityMonitor& getQualityMonitor()
{
    static QualityMonitor instance;
    return instance;
}