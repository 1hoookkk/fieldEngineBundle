#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <vector>
#include <memory>
#include <string>
#include <chrono>

/**
 ╔══════════════════════════════════════════════════════════════════════════════╗
 ║                        CRITICAL QUALITY GUARDIAN                            ║
 ║                   Phase 1: Static Analysis Fortress                         ║
 ║                                                                              ║
 ║  🛡️ MISSION: Prevent SpectralCanvas Pro crashes through systematic          ║
 ║      quality monitoring, graceful degradation, and real-time validation    ║
 ║                                                                              ║
 ║  🚨 DEPLOYMENT: HIGHEST PRIORITY - Deploy before completing build           ║
 ╚══════════════════════════════════════════════════════════════════════════════╝
 */

namespace SpectralCanvasQuality {

//==============================================================================
// CRASH PREVENTION SYSTEM
//==============================================================================

enum class CrashRiskLevel
{
    SAFE = 0,           // No detected risks
    LOW = 1,            // Minor issues, monitoring
    MEDIUM = 2,         // Potential risks, degraded mode recommended
    HIGH = 3,           // Critical risks, immediate action required
    CRITICAL = 4        // Imminent crash risk, emergency protocols
};

enum class ComponentStatus
{
    HEALTHY = 0,        // Component functioning normally
    DEGRADED = 1,       // Component running with reduced functionality
    FAILING = 2,        // Component experiencing errors but still functional
    FAILED = 3,         // Component has failed, fallback active
    OFFLINE = 4         // Component disabled/unavailable
};

struct QualityMetrics
{
    // Memory Management
    std::atomic<size_t> totalMemoryUsage{0};
    std::atomic<size_t> peakMemoryUsage{0};
    std::atomic<int> memoryLeakCount{0};
    std::atomic<int> nullPointerDetections{0};
    
    // Performance Monitoring
    std::atomic<double> averageCpuUsage{0.0};
    std::atomic<double> peakCpuUsage{0.0};
    std::atomic<int> audioDropouts{0};
    std::atomic<double> averageLatency{0.0};
    
    // Error Tracking
    std::atomic<int> totalErrors{0};
    std::atomic<int> criticalErrors{0};
    std::atomic<int> recoveredErrors{0};
    std::atomic<int> componentFailures{0};
    
    // System Health
    std::atomic<double> systemHealthPercentage{100.0};
    std::atomic<CrashRiskLevel> currentRiskLevel{CrashRiskLevel::SAFE};
    
    void reset() noexcept
    {
        totalMemoryUsage = 0;
        peakMemoryUsage = 0;
        memoryLeakCount = 0;
        nullPointerDetections = 0;
        averageCpuUsage = 0.0;
        peakCpuUsage = 0.0;
        audioDropouts = 0;
        averageLatency = 0.0;
        totalErrors = 0;
        criticalErrors = 0;
        recoveredErrors = 0;
        componentFailures = 0;
        systemHealthPercentage = 100.0;
        currentRiskLevel = CrashRiskLevel::SAFE;
    }
};

//==============================================================================
// COMPONENT MONITORING SYSTEM
//==============================================================================

struct ComponentHealth
{
    std::string componentName;
    ComponentStatus status = ComponentStatus::HEALTHY;
    std::atomic<int> errorCount{0};
    std::atomic<int> crashCount{0};
    std::chrono::steady_clock::time_point lastHealthCheck;
    std::string lastError = "";
    bool isEssential = true;  // Essential components trigger degraded mode if failed
    
    ComponentHealth(const std::string& name, bool essential = true)
        : componentName(name), isEssential(essential)
    {
        lastHealthCheck = std::chrono::steady_clock::now();
    }
    
    void recordError(const std::string& error)
    {
        errorCount++;
        lastError = error;
        lastHealthCheck = std::chrono::steady_clock::now();
        
        // Escalate status based on error count
        if (errorCount >= 10)
            status = ComponentStatus::FAILING;
        else if (errorCount >= 5)
            status = ComponentStatus::DEGRADED;
    }
    
    void recordCrash()
    {
        crashCount++;
        status = ComponentStatus::FAILED;
        lastHealthCheck = std::chrono::steady_clock::now();
    }
    
    void markHealthy()
    {
        status = ComponentStatus::HEALTHY;
        lastHealthCheck = std::chrono::steady_clock::now();
    }
};

//==============================================================================
// GRACEFUL DEGRADATION MANAGER
//==============================================================================

class DegradedModeManager
{
public:
    enum class DegradedMode
    {
        NORMAL = 0,         // All systems operational
        AUDIO_DEGRADED = 1, // Audio system in safe mode
        UI_DEGRADED = 2,    // UI in minimal mode
        CANVAS_DEGRADED = 3,// Paint system limited
        EMERGENCY_MODE = 4  // Critical systems only
    };
    
    DegradedModeManager();
    ~DegradedModeManager() = default;
    
    // Mode Management
    void activateDegradedMode(DegradedMode mode, const std::string& reason);
    void deactivateDegradedMode();
    DegradedMode getCurrentMode() const { return currentMode.load(); }
    bool isDegraded() const { return currentMode.load() != DegradedMode::NORMAL; }
    
    // Component Fallback
    bool shouldUseAudioFallback() const;
    bool shouldUseUIFallback() const;
    bool shouldUseCanvasFallback() const;
    
    // Status
    std::string getDegradationReason() const { return degradationReason; }
    
private:
    std::atomic<DegradedMode> currentMode{DegradedMode::NORMAL};
    std::string degradationReason = "";
    std::chrono::steady_clock::time_point degradationStartTime;
};

//==============================================================================
// REAL-TIME QUALITY MONITOR
//==============================================================================

class QualityMonitor
{
public:
    QualityMonitor();
    ~QualityMonitor();
    
    // System Registration
    void registerComponent(const std::string& name, bool essential = true);
    void unregisterComponent(const std::string& name);
    
    // Health Monitoring
    void reportComponentError(const std::string& component, const std::string& error);
    void reportComponentCrash(const std::string& component);
    void reportComponentHealthy(const std::string& component);
    
    // Memory Monitoring
    void updateMemoryUsage(size_t currentUsage);
    void reportMemoryLeak(size_t leakSize);
    void reportNullPointerAccess(const std::string& location);
    
    // Performance Monitoring  
    void updateCpuUsage(double usage);
    void reportAudioDropout();
    void updateLatency(double latencyMs);
    
    // Risk Assessment
    CrashRiskLevel assessCrashRisk();
    double calculateSystemHealth();
    std::vector<std::string> getSystemReport();
    
    // Emergency Protocols
    bool shouldActivateEmergencyMode();
    void activateEmergencyProtocols();
    
    // Getters
    const QualityMetrics& getMetrics() const { return metrics; }
    const std::vector<std::shared_ptr<ComponentHealth>>& getComponents() const { return components; }
    
private:
    QualityMetrics metrics;
    std::vector<std::shared_ptr<ComponentHealth>> components;
    std::unique_ptr<DegradedModeManager> degradedModeManager;
    
    // Thresholds
    static constexpr size_t MEMORY_LEAK_THRESHOLD = 1024 * 1024; // 1MB
    static constexpr double CPU_USAGE_THRESHOLD = 80.0;          // 80%
    static constexpr double LATENCY_THRESHOLD = 50.0;            // 50ms
    static constexpr int ERROR_COUNT_THRESHOLD = 5;
    
    // Helper Methods
    std::shared_ptr<ComponentHealth> findComponent(const std::string& name);
    void updateSystemHealth();
    void checkThresholds();
};

//==============================================================================
// STATIC ANALYSIS INTEGRATION
//==============================================================================

class StaticAnalysisChecker
{
public:
    struct AnalysisResult
    {
        enum class Severity
        {
            INFO = 0,
            WARNING = 1,
            ERROR = 2,
            CRITICAL = 3
        };
        
        Severity severity;
        std::string category;
        std::string message;
        std::string filename;
        int lineNumber;
        std::string function;
    };
    
    static std::vector<AnalysisResult> performStaticAnalysis();
    static bool hasCriticalIssues(const std::vector<AnalysisResult>& results);
    static std::string generateReport(const std::vector<AnalysisResult>& results);
    
private:
    // Static analysis rules for crash prevention
    static void checkNullPointerRisks(std::vector<AnalysisResult>& results);
    static void checkBufferOverflowRisks(std::vector<AnalysisResult>& results);
    static void checkMemoryLeakRisks(std::vector<AnalysisResult>& results);
    static void checkThreadSafetyRisks(std::vector<AnalysisResult>& results);
    static void checkResourceManagementRisks(std::vector<AnalysisResult>& results);
};

//==============================================================================
// QUALITY GUARDIAN MASTER CONTROLLER
//==============================================================================

class QualityGuardian
{
public:
    static QualityGuardian& getInstance()
    {
        static QualityGuardian instance;
        return instance;
    }
    
    // System Lifecycle
    bool initialize();
    void shutdown();
    bool isInitialized() const { return initialized.load(); }
    
    // Component Registration
    void registerCriticalComponent(const std::string& name);
    void registerOptionalComponent(const std::string& name);
    
    // Real-time Monitoring
    void startMonitoring();
    void stopMonitoring();
    bool isMonitoring() const { return monitoring.load(); }
    
    // Quality Checks
    bool performStartupQualityCheck();
    bool performRuntimeQualityCheck();
    
    // Emergency Response
    void handleCriticalError(const std::string& component, const std::string& error);
    void handleComponentCrash(const std::string& component);
    
    // Reporting
    std::string generateFullReport();
    double getCurrentSystemHealth();
    CrashRiskLevel getCurrentRiskLevel();
    
    // Safe Access to Subsystems
    QualityMonitor* getMonitor() { return monitor.get(); }
    DegradedModeManager* getDegradedModeManager() { return degradedModeManager.get(); }
    
private:
    QualityGuardian() = default;
    ~QualityGuardian() = default;
    
    // Singleton enforcement
    QualityGuardian(const QualityGuardian&) = delete;
    QualityGuardian& operator=(const QualityGuardian&) = delete;
    
    // Core Components
    std::unique_ptr<QualityMonitor> monitor;
    std::unique_ptr<DegradedModeManager> degradedModeManager;
    
    // State
    std::atomic<bool> initialized{false};
    std::atomic<bool> monitoring{false};
    
    // Timing
    std::chrono::steady_clock::time_point initializationTime;
    std::chrono::steady_clock::time_point lastQualityCheck;
};

} // namespace SpectralCanvasQuality

//==============================================================================
// CONVENIENCE MACROS FOR QUALITY MONITORING
//==============================================================================

#define QUALITY_GUARD SpectralCanvasQuality::QualityGuardian::getInstance()

#define QUALITY_REGISTER_CRITICAL(component) \
    QUALITY_GUARD.registerCriticalComponent(component)

#define QUALITY_REGISTER_OPTIONAL(component) \
    QUALITY_GUARD.registerOptionalComponent(component)

#define QUALITY_REPORT_ERROR(component, error) \
    QUALITY_GUARD.handleCriticalError(component, error)

#define QUALITY_REPORT_CRASH(component) \
    QUALITY_GUARD.handleComponentCrash(component)

#define QUALITY_CHECK_STARTUP() \
    QUALITY_GUARD.performStartupQualityCheck()

#define QUALITY_CHECK_RUNTIME() \
    QUALITY_GUARD.performRuntimeQualityCheck()

#define QUALITY_SYSTEM_HEALTH() \
    QUALITY_GUARD.getCurrentSystemHealth()

// Null pointer safety check
#define QUALITY_NULL_CHECK(ptr, component) \
    if (!(ptr)) { \
        QUALITY_REPORT_ERROR(component, "Null pointer detected: " #ptr); \
        return; \
    }

// Safe function call with error handling
#define QUALITY_SAFE_CALL(call, component) \
    try { \
        call; \
    } catch (const std::exception& e) { \
        QUALITY_REPORT_ERROR(component, std::string("Exception: ") + e.what()); \
    } catch (...) { \
        QUALITY_REPORT_ERROR(component, "Unknown exception in " #call); \
    }