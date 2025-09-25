/******************************************************************************
 * File: SystemIntegrityTest.h
 * Description: Comprehensive system integrity testing framework
 * 
 * Code Quality Guardian - Systematic validation of all critical paths
 * 
 * Copyright (c) 2025 Spectral Audio Systems
 ******************************************************************************/

#pragma once

#include <JuceHeader.h>
#include <vector>
#include <string>
#include <functional>
#include <chrono>
#include <memory>

//==============================================================================
// Test Result Types
//==============================================================================

enum class TestResult
{
    NotRun,
    Passed,
    Failed,
    Skipped,
    Warning
};

struct TestCase
{
    std::string name;
    std::string description;
    std::function<TestResult()> testFunction;
    bool isCritical = true;
    float timeoutSeconds = 30.0f;
    TestResult result = TestResult::NotRun;
    std::string errorMessage;
    float executionTimeMs = 0.0f;
};

struct TestSuite
{
    std::string name;
    std::string description;
    std::vector<TestCase> tests;
    int passedCount = 0;
    int failedCount = 0;
    int skippedCount = 0;
    int warningCount = 0;
    float totalExecutionTimeMs = 0.0f;
};

//==============================================================================
// System Integrity Test Framework
//==============================================================================

class SystemIntegrityTest
{
public:
    SystemIntegrityTest();
    ~SystemIntegrityTest();
    
    // Test Suite Management
    void addTestSuite(const TestSuite& suite);
    void clearTestSuites();
    
    // Test Execution
    bool runAllTests();
    bool runTestSuite(const std::string& suiteName);
    bool runSingleTest(const std::string& suiteName, const std::string& testName);
    
    // Results and Reporting
    std::string generateFullReport() const;
    std::string generateSummaryReport() const;
    std::vector<std::string> getCriticalFailures() const;
    bool hasAnyCriticalFailures() const;
    float getOverallSuccessRate() const;
    
    // Test Configuration
    void setStopOnFirstFailure(bool stop) { stopOnFirstFailure = stop; }
    void setVerboseOutput(bool verbose) { verboseOutput = verbose; }
    void setTimeout(float timeoutSeconds) { defaultTimeout = timeoutSeconds; }
    
    // Built-in Test Suites
    void createStartupIntegrityTests();
    void createAudioEngineTests();
    void createPaintSystemTests();
    void createMemoryManagementTests();
    void createThreadSafetyTests();
    void createPerformanceTests();
    void createRecoveryTests();

private:
    // Test Execution Helpers
    TestResult executeTest(TestCase& testCase);
    void updateSuiteResults(TestSuite& suite);
    
    // Built-in Test Implementations
    TestResult testSpectralEngineInitialization();
    TestResult testAudioDeviceInitialization();
    TestResult testPaintToAudioConversion();
    TestResult testSampleLoading();
    TestResult testParameterThreadSafety();
    TestResult testMemoryLeakDetection();
    TestResult testErrorRecovery();
    TestResult testDegradedModeActivation();
    TestResult testPerformanceThresholds();
    TestResult testUIResponsiveness();
    
    // Data Members
    std::vector<TestSuite> testSuites;
    bool stopOnFirstFailure = false;
    bool verboseOutput = true;
    float defaultTimeout = 30.0f;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SystemIntegrityTest)
};

//==============================================================================
// Automated Test Runner
//==============================================================================

class AutomatedTestRunner : public juce::Timer
{
public:
    AutomatedTestRunner();
    ~AutomatedTestRunner() override;
    
    // Test Scheduling
    void scheduleStartupTests();
    void schedulePeriodicTests(int intervalMinutes = 60);
    void scheduleStressTests(int durationMinutes = 10);
    
    // Test Management
    void startContinuousMonitoring();
    void stopContinuousMonitoring();
    bool isMonitoringActive() const { return monitoringActive.load(); }
    
    // Results Management
    void setResultCallback(std::function<void(const std::string&)> callback);
    void exportTestHistory(const juce::File& file) const;

private:
    void timerCallback() override;
    void runScheduledTests();
    void runPerformanceMonitoring();
    void runStabilityChecks();
    
    SystemIntegrityTest testFramework;
    std::atomic<bool> monitoringActive{false};
    std::function<void(const std::string&)> resultCallback;
    
    int periodicTestInterval = 60; // minutes
    std::chrono::steady_clock::time_point lastPeriodicTest;
    std::chrono::steady_clock::time_point lastStabilityCheck;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AutomatedTestRunner)
};

//==============================================================================
// Test Utilities and Helpers
//==============================================================================

class TestUtilities
{
public:
    // Memory Testing
    static size_t getCurrentMemoryUsage();
    static bool detectMemoryLeaks(std::function<void()> operation, size_t toleranceMB = 1);
    
    // Performance Testing
    static float measureExecutionTime(std::function<void()> operation);
    static bool verifyPerformanceThreshold(std::function<void()> operation, float maxTimeMs);
    
    // Audio Testing
    static bool verifyAudioOutput(std::function<void(juce::AudioBuffer<float>&)> audioFunction,
                                 float expectedFrequency, float toleranceHz = 5.0f);
    static bool detectAudioArtifacts(const juce::AudioBuffer<float>& buffer);
    
    // Thread Safety Testing
    static bool testConcurrentAccess(std::function<void()> operation, int threadCount = 4, int iterations = 1000);
    
    // Component Testing
    static bool verifyComponentInitialization(const std::string& componentName,
                                            std::function<bool()> initFunction,
                                            std::function<bool()> verifyFunction);
    
    // Error Injection Testing
    static void simulateMemoryPressure(size_t targetMB, float durationSeconds);
    static void simulateHighCPULoad(float targetPercent, float durationSeconds);
    static void simulateAudioDeviceFailure();
    
private:
    TestUtilities() = delete; // Static utility class
};

//==============================================================================
// Global Test Framework Access
//==============================================================================

// Get global test framework instance
SystemIntegrityTest& getSystemIntegrityTest();
AutomatedTestRunner& getAutomatedTestRunner();

// Convenience macros for test creation
#define DEFINE_TEST_CASE(name, description, critical) \
    TestCase{name, description, [this]() -> TestResult { return test##name(); }, critical}

#define EXPECT_TRUE(condition, message) \
    do { \
        if (!(condition)) { \
            return TestResult::Failed; \
        } \
    } while(0)

#define EXPECT_FALSE(condition, message) \
    do { \
        if (condition) { \
            return TestResult::Failed; \
        } \
    } while(0)

#define EXPECT_NO_THROW(operation, message) \
    do { \
        try { \
            operation; \
        } catch (...) { \
            return TestResult::Failed; \
        } \
    } while(0)

#define EXPECT_PERFORMANCE(operation, maxTimeMs, message) \
    do { \
        if (!TestUtilities::verifyPerformanceThreshold([&]() { operation; }, maxTimeMs)) { \
            return TestResult::Failed; \
        } \
    } while(0)

//==============================================================================
// Integration with Quality Monitor
//==============================================================================

class QualityTestIntegration
{
public:
    static void integrateWithQualityMonitor();
    static void reportTestResults(const std::string& suiteName, const TestSuite& results);
    static void createQualityAlerts(const std::vector<std::string>& criticalFailures);
};

//==============================================================================
// Startup Safety Test Implementation
//==============================================================================

class StartupSafetyTest
{
public:
    static TestResult validateSpectralEngineStartup();
    static TestResult validateAudioSystemInitialization();
    static TestResult validateUIComponentCreation();
    static TestResult validateDegradedModeHandling();
    static TestResult validateExceptionSafety();
    static TestResult validateResourceCleanup();
    
    // Specific test scenarios
    static TestResult testStartupWithNoAudioDevice();
    static TestResult testStartupWithInsufficientMemory();
    static TestResult testStartupWithCorruptedSettings();
    static TestResult testStartupWithMissingComponents();
    
    // Recovery testing
    static TestResult testRecoveryFromStartupFailure();
    static TestResult testGracefulDegradation();
    static TestResult testFallbackSystemActivation();
};

//==============================================================================
// Performance Validation Test
//==============================================================================

class PerformanceValidationTest
{
public:
    static TestResult validateAudioLatency();
    static TestResult validateCPUUsage();
    static TestResult validateMemoryUsage();
    static TestResult validateUIResponsiveness();
    static TestResult validatePaintToAudioLatency();
    
    // Stress testing
    static TestResult stressTestCPUUsage(int durationSeconds = 30);
    static TestResult stressTestMemoryAllocation(int durationSeconds = 30);
    static TestResult stressTestConcurrentOperations(int durationSeconds = 30);
    
    // Regression testing
    static TestResult detectPerformanceRegression();
    static TestResult validatePerformanceStability();
};

//==============================================================================
// Thread Safety Validation
//==============================================================================

class ThreadSafetyTest
{
public:
    static TestResult validateAudioThreadSafety();
    static TestResult validateUIThreadSafety();
    static TestResult validateParameterThreadSafety();
    static TestResult validatePaintEventThreadSafety();
    
    // Race condition testing
    static TestResult testConcurrentParameterAccess();
    static TestResult testConcurrentAudioProcessing();
    static TestResult testConcurrentPaintEvents();
    
    // Deadlock detection
    static TestResult detectPotentialDeadlocks();
    static TestResult validateLockOrdering();
};