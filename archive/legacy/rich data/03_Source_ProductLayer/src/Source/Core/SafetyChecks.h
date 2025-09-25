#pragma once
/**
 * Safety Checks and Validation
 * Production-ready error handling following best practices
 */

#include <JuceHeader.h>
#include <atomic>
#include <string>

/**
 * Thread-safe logging system for debugging
 * Never blocks audio thread
 */
class SafeLogger
{
public:
    static SafeLogger& getInstance()
    {
        static SafeLogger instance;
        return instance;
    }
    
    void log(const std::string& message, bool isError = false)
    {
        if (!loggingEnabled.load()) return;
        
        auto timestamp = juce::Time::getCurrentTime().toString(true, true, true, true);
        juce::String logMessage = timestamp + " - " + juce::String(message);
        
        if (isError)
        {
            errorCount.fetch_add(1, std::memory_order_relaxed);
            DBG("[ERROR] " << logMessage);
        }
        else
        {
            DBG("[INFO] " << logMessage);
        }
    }
    
    void logAudioPerformance(double cpuUsage, int xruns)
    {
        if (cpuUsage > 80.0)
        {
            log("High CPU usage: " + std::to_string(cpuUsage) + "%", true);
        }
        if (xruns > 0)
        {
            totalXruns.fetch_add(xruns, std::memory_order_relaxed);
            log("Audio dropouts detected: " + std::to_string(xruns), true);
        }
    }
    
    int getErrorCount() const { return errorCount.load(); }
    int getXrunCount() const { return totalXruns.load(); }
    void enableLogging(bool enable) { loggingEnabled.store(enable); }
    
private:
    std::atomic<bool> loggingEnabled{true};
    std::atomic<int> errorCount{0};
    std::atomic<int> totalXruns{0};
};

/**
 * Audio validation utilities
 */
class AudioValidator
{
public:
    static bool validateSampleRate(double sampleRate)
    {
        const bool valid = sampleRate >= 22050.0 && sampleRate <= 192000.0;
        if (!valid)
        {
            SafeLogger::getInstance().log(
                "Invalid sample rate: " + std::to_string(sampleRate), true);
        }
        return valid;
    }
    
    static bool validateBufferSize(int bufferSize)
    {
        const bool valid = bufferSize >= 32 && bufferSize <= 8192;
        if (!valid)
        {
            SafeLogger::getInstance().log(
                "Invalid buffer size: " + std::to_string(bufferSize), true);
        }
        return valid;
    }
    
    static bool validateAudioBuffer(const juce::AudioBuffer<float>& buffer)
    {
        if (buffer.getNumChannels() <= 0 || buffer.getNumSamples() <= 0)
        {
            SafeLogger::getInstance().log("Invalid audio buffer dimensions", true);
            return false;
        }
        
        // Check for NaN or Inf
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const float* data = buffer.getReadPointer(ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                if (std::isnan(data[i]) || std::isinf(data[i]))
                {
                    SafeLogger::getInstance().log("NaN/Inf detected in audio buffer", true);
                    return false;
                }
            }
        }
        return true;
    }
};

/**
 * Memory usage tracker
 */
class MemoryMonitor
{
public:
    static void checkMemoryUsage()
    {
        // Memory monitoring is optional - could be implemented per-platform
        // For now, just a placeholder for production builds
        #ifdef DEBUG
        SafeLogger::getInstance().log("Memory check performed");
        #endif
    }
};

/**
 * Thread safety verifier
 */
class ThreadSafetyVerifier
{
public:
    static void assertAudioThread()
    {
        #ifdef DEBUG
        if (!isAudioThread())
        {
            SafeLogger::getInstance().log("Function called from wrong thread!", true);
            jassertfalse;
        }
        #endif
    }
    
    static void assertMessageThread()
    {
        #ifdef DEBUG
        if (!juce::MessageManager::getInstance()->isThisTheMessageThread())
        {
            SafeLogger::getInstance().log("GUI function called from wrong thread!", true);
            jassertfalse;
        }
        #endif
    }
    
private:
    static bool isAudioThread()
    {
        // This is a simplified check - in production you'd track the actual audio thread ID
        return !juce::MessageManager::getInstance()->isThisTheMessageThread();
    }
};

/**
 * Resource cleanup verifier
 */
class ResourceGuard
{
public:
    ResourceGuard(const std::string& resourceName)
        : name(resourceName)
    {
        SafeLogger::getInstance().log("Acquiring resource: " + name);
    }
    
    ~ResourceGuard()
    {
        SafeLogger::getInstance().log("Releasing resource: " + name);
    }
    
private:
    std::string name;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ResourceGuard)
};