/******************************************************************************
 * File: DegradedModeManager.h
 * Description: Degraded mode management for graceful component failure handling
 * 
 * Code Quality Guardian - Graceful degradation when components fail
 * 
 * Copyright (c) 2025 Spectral Audio Systems
 ******************************************************************************/

#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <memory>
#include <unordered_map>
#include <string>
#include <functional>

//==============================================================================
// Component Status Tracking
//==============================================================================

enum class ComponentStatus
{
    Unknown,
    Initializing,
    Healthy,
    Degraded,
    Failed,
    Disabled
};

struct ComponentHealth
{
    ComponentStatus status = ComponentStatus::Unknown;
    std::string errorMessage;
    std::chrono::steady_clock::time_point lastUpdate;
    float reliability = 1.0f; // 0.0 = completely unreliable, 1.0 = fully reliable
    int failureCount = 0;
    bool criticalForOperation = true;
};

//==============================================================================
// Degraded Mode Configuration
//==============================================================================

enum class DegradedModeLevel
{
    FullFunctionality,    // All components working
    MinorDegradation,     // Non-critical components failed
    MajorDegradation,     // Some critical components failed
    EmergencyMode,        // Only basic functionality available
    SafeMode             // Minimal operation, avoid crashes
};

struct DegradedModeConfig
{
    DegradedModeLevel currentLevel = DegradedModeLevel::FullFunctionality;
    bool allowAudioGeneration = true;
    bool allowPaintInteraction = true;
    bool allowSampleLoading = true;
    bool allowParameterChanges = true;
    bool showWarningMessages = true;
    bool enableFallbackSystems = true;
};

//==============================================================================
// Fallback System Interface
//==============================================================================

class IFallbackSystem
{
public:
    virtual ~IFallbackSystem() = default;
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual bool isOperational() const = 0;
    virtual std::string getSystemName() const = 0;
    virtual DegradedModeLevel getRequiredDegradationLevel() const = 0;
};

//==============================================================================
// Fallback Audio Processor
//==============================================================================

class FallbackAudioProcessor : public IFallbackSystem
{
public:
    FallbackAudioProcessor();
    ~FallbackAudioProcessor() override;
    
    // IFallbackSystem interface
    bool initialize() override;
    void shutdown() override;
    bool isOperational() const override;
    std::string getSystemName() const override { return "FallbackAudioProcessor"; }
    DegradedModeLevel getRequiredDegradationLevel() const override { return DegradedModeLevel::MajorDegradation; }
    
    // Audio processing
    void processBlock(juce::AudioBuffer<float>& buffer);
    void prepareToPlay(double sampleRate, int bufferSize, int numChannels);
    void releaseResources();
    
    // Simple test tone generation
    void setTestToneEnabled(bool enabled) { testToneEnabled.store(enabled); }
    void setTestToneFrequency(float frequency) { testToneFrequency.store(frequency); }
    void setTestToneAmplitude(float amplitude) { testToneAmplitude.store(amplitude); }

private:
    std::atomic<bool> isInitialized{false};
    std::atomic<bool> testToneEnabled{false};
    std::atomic<float> testToneFrequency{440.0f};
    std::atomic<float> testToneAmplitude{0.1f};
    std::atomic<float> testTonePhase{0.0f};
    
    double currentSampleRate = 44100.0;
};

//==============================================================================
// Fallback Paint System
//==============================================================================

class FallbackPaintSystem : public IFallbackSystem
{
public:
    FallbackPaintSystem();
    ~FallbackPaintSystem() override;
    
    // IFallbackSystem interface
    bool initialize() override;
    void shutdown() override;
    bool isOperational() const override;
    std::string getSystemName() const override { return "FallbackPaintSystem"; }
    DegradedModeLevel getRequiredDegradationLevel() const override { return DegradedModeLevel::MinorDegradation; }
    
    // Simple paint event handling
    void processPaintEvent(float x, float y, float pressure, juce::Colour color);
    void clearCanvas();
    
    // Basic visual feedback
    std::vector<juce::Point<float>> getRecentPaintPoints() const;

private:
    std::atomic<bool> isInitialized{false};
    std::vector<juce::Point<float>> paintHistory;
    juce::CriticalSection paintLock;
    
    static constexpr size_t MAX_PAINT_HISTORY = 1000;
};

//==============================================================================
// Degraded Mode Manager - Main Class
//==============================================================================

class DegradedModeManager
{
public:
    DegradedModeManager();
    ~DegradedModeManager();
    
    // Component Health Management
    void registerComponent(const std::string& componentName, bool criticalForOperation = true);
    void updateComponentStatus(const std::string& componentName, ComponentStatus status, 
                              const std::string& errorMessage = "");
    ComponentHealth getComponentHealth(const std::string& componentName) const;
    std::vector<std::string> getFailedComponents() const;
    std::vector<std::string> getCriticalFailures() const;
    
    // Degraded Mode Management
    DegradedModeLevel getCurrentDegradationLevel() const;
    DegradedModeConfig getCurrentConfiguration() const;
    bool isFeatureAvailable(const std::string& featureName) const;
    
    // Fallback System Management
    void registerFallbackSystem(std::unique_ptr<IFallbackSystem> fallbackSystem);
    bool activateFallbackSystem(const std::string& systemName);
    void deactivateFallbackSystem(const std::string& systemName);
    bool isFallbackSystemActive(const std::string& systemName) const;
    
    // Recovery Management
    bool attemptComponentRecovery(const std::string& componentName);
    void setRecoveryCallback(const std::string& componentName, 
                           std::function<bool()> recoveryCallback);
    
    // User Interface Support
    std::string getDegradationStatusMessage() const;
    std::vector<std::string> getUserWarnings() const;
    bool shouldShowDegradationWarning() const;
    
    // System Health Assessment
    float calculateSystemReliability() const;
    bool isSystemStable() const;
    std::string generateDegradationReport() const;
    
    // Emergency Operations
    void enterEmergencyMode(const std::string& reason);
    void enterSafeMode(const std::string& reason);
    bool canExitDegradedMode() const;
    void attemptSystemRecovery();

private:
    // Internal Methods
    void assessDegradationLevel();
    void updateConfiguration();
    void activateAppropriateeFallbacks();
    void notifyDegradationChange(DegradedModeLevel oldLevel, DegradedModeLevel newLevel);
    
    // Recovery Management
    void scheduleRecoveryAttempt(const std::string& componentName);
    void performScheduledRecovery();
    
    // Data Members
    std::unordered_map<std::string, ComponentHealth> componentHealth;
    std::unordered_map<std::string, std::function<bool()>> recoveryCallbacks;
    std::unordered_map<std::string, std::unique_ptr<IFallbackSystem>> fallbackSystems;
    std::unordered_map<std::string, bool> activeFallbacks;
    
    DegradedModeConfig currentConfig;
    std::atomic<DegradedModeLevel> currentDegradationLevel{DegradedModeLevel::FullFunctionality};
    
    // Feature availability map
    std::unordered_map<std::string, bool> featureAvailability;
    
    // Emergency mode tracking
    std::atomic<bool> emergencyModeActive{false};
    std::atomic<bool> safeModeActive{false};
    std::string emergencyReason;
    std::string safeModeReason;
    
    // Thread safety
    mutable juce::CriticalSection componentLock;
    mutable juce::CriticalSection fallbackLock;
    mutable juce::CriticalSection configLock;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DegradedModeManager)
};

//==============================================================================
// Global Degraded Mode Manager Access
//==============================================================================

// Singleton access to degraded mode manager
DegradedModeManager& getDegradedModeManager();

// Convenience macros for degraded mode operations
#define DEGRADED_MODE_REGISTER_COMPONENT(name, critical) \
    getDegradedModeManager().registerComponent(name, critical)

#define DEGRADED_MODE_UPDATE_STATUS(component, status, error) \
    getDegradedModeManager().updateComponentStatus(component, status, error)

#define DEGRADED_MODE_CHECK_FEATURE(feature) \
    getDegradedModeManager().isFeatureAvailable(feature)

#define DEGRADED_MODE_EMERGENCY(reason) \
    getDegradedModeManager().enterEmergencyMode(reason)

#define DEGRADED_MODE_IS_STABLE() \
    getDegradedModeManager().isSystemStable()

//==============================================================================
// Component Status Helper Macros
//==============================================================================

#define COMPONENT_INITIALIZED(name) \
    DEGRADED_MODE_UPDATE_STATUS(name, ComponentStatus::Healthy, "")

#define COMPONENT_FAILED(name, error) \
    DEGRADED_MODE_UPDATE_STATUS(name, ComponentStatus::Failed, error)

#define COMPONENT_DEGRADED(name, error) \
    DEGRADED_MODE_UPDATE_STATUS(name, ComponentStatus::Degraded, error)

#define COMPONENT_DISABLED(name, reason) \
    DEGRADED_MODE_UPDATE_STATUS(name, ComponentStatus::Disabled, reason)

//==============================================================================
// Safe Operation Helpers
//==============================================================================

template<typename T>
bool safeExecute(const std::string& operationName, T&& operation)
{
    try
    {
        operation();
        return true;
    }
    catch (const std::exception& e)
    {
        COMPONENT_FAILED(operationName, e.what());
        return false;
    }
    catch (...)
    {
        COMPONENT_FAILED(operationName, "Unknown exception");
        return false;
    }
}

template<typename T, typename FallbackT>
auto safeExecuteWithFallback(const std::string& operationName, 
                             T&& operation, 
                             FallbackT&& fallback) -> decltype(operation())
{
    try
    {
        return operation();
    }
    catch (const std::exception& e)
    {
        COMPONENT_DEGRADED(operationName, e.what());
        return fallback();
    }
    catch (...)
    {
        COMPONENT_DEGRADED(operationName, "Unknown exception");
        return fallback();
    }
}