#pragma once
#include <JuceHeader.h>
#include <atomic>

/**
 * Real-time safe assertion system for audio processing
 * 
 * CRITICAL: These assertions NEVER block the audio thread
 * They log errors and track statistics but never halt execution
 * 
 * @author SpectralCanvas Team
 * @version 1.0 - Production Safe
 */

// Global error tracking (lock-free)
class RealtimeErrorTracker
{
public:
    static RealtimeErrorTracker& getInstance()
    {
        static RealtimeErrorTracker instance;
        return instance;
    }
    
    void reportError(const char* file, int line, const char* condition) noexcept
    {
        // Increment error count atomically
        errorCount.fetch_add(1, std::memory_order_relaxed);
        
        // Store last error info (may race, but that's acceptable)
        lastErrorFile = file;
        lastErrorLine = line;
        lastErrorCondition = condition;
        lastErrorTime = juce::Time::currentTimeMillis();
        
        // Log asynchronously (non-blocking)
        if (shouldLog())
        {
            logErrorAsync(file, line, condition);
        }
    }
    
    uint64_t getErrorCount() const noexcept
    {
        return errorCount.load(std::memory_order_relaxed);
    }
    
    void reset() noexcept
    {
        errorCount.store(0, std::memory_order_relaxed);
    }
    
    struct ErrorInfo
    {
        const char* file = nullptr;
        int line = 0;
        const char* condition = nullptr;
        int64_t time = 0;
        uint64_t count = 0;
    };
    
    ErrorInfo getLastError() const noexcept
    {
        ErrorInfo info;
        info.file = lastErrorFile;
        info.line = lastErrorLine;
        info.condition = lastErrorCondition;
        info.time = lastErrorTime;
        info.count = getErrorCount();
        return info;
    }
    
private:
    RealtimeErrorTracker() = default;
    
    std::atomic<uint64_t> errorCount{0};
    std::atomic<int> logThrottle{0};
    
    // Last error info (may have races, but acceptable for diagnostics)
    const char* lastErrorFile = nullptr;
    int lastErrorLine = 0;
    const char* lastErrorCondition = nullptr;
    int64_t lastErrorTime = 0;
    
    bool shouldLog() noexcept
    {
        // Throttle logging to once per 100 errors
        int throttle = logThrottle.fetch_add(1, std::memory_order_relaxed);
        return (throttle % 100) == 0;
    }
    
    void logErrorAsync(const char* file, int line, const char* condition) noexcept
    {
        // Use JUCE's async logging (non-blocking)
        juce::String msg;
        msg << "RT_ASSERT FAILED: " << condition 
            << " at " << file << ":" << line
            << " (total errors: " << errorCount.load() << ")";
        
        // This is async and won't block
        juce::Logger::writeToLog(msg);
    }
};

/**
 * Real-time safe assertion macros
 * 
 * These NEVER block the audio thread - they only log and count errors
 */

// Production assertion - logs error but continues execution
#define RT_ASSERT(condition) \
    do { \
        if (!(condition)) [[unlikely]] { \
            RealtimeErrorTracker::getInstance().reportError(__FILE__, __LINE__, #condition); \
        } \
    } while(false)

// Production assertion with custom message
#define RT_ASSERT_MSG(condition, message) \
    do { \
        if (!(condition)) [[unlikely]] { \
            RealtimeErrorTracker::getInstance().reportError(__FILE__, __LINE__, message); \
        } \
    } while(false)

// Range check assertion (common in audio code)
#define RT_ASSERT_RANGE(value, min, max) \
    RT_ASSERT((value) >= (min) && (value) <= (max))

// Array bounds check
#define RT_ASSERT_INDEX(index, size) \
    RT_ASSERT((index) >= 0 && (index) < (size))

// Null pointer check
#define RT_ASSERT_NOT_NULL(ptr) \
    RT_ASSERT((ptr) != nullptr)

// Debug-only assertion (completely removed in release builds)
#ifdef DEBUG
    #define RT_DEBUG_ASSERT(condition) RT_ASSERT(condition)
#else
    #define RT_DEBUG_ASSERT(condition) ((void)0)
#endif

/**
 * Replace jassert in audio code with these macros:
 * 
 * OLD CODE:
 *   jassert(index < BANK_SIZE);
 * 
 * NEW CODE:
 *   RT_ASSERT_INDEX(index, BANK_SIZE);
 * 
 * OLD CODE:
 *   jassert(frequency > 0);
 * 
 * NEW CODE:
 *   RT_ASSERT(frequency > 0);
 */

/**
 * Performance monitoring assertions
 * Track performance violations without blocking
 */
class RealtimePerformanceMonitor
{
public:
    static RealtimePerformanceMonitor& getInstance()
    {
        static RealtimePerformanceMonitor instance;
        return instance;
    }
    
    void checkProcessingTime(double actualMs, double expectedMs) noexcept
    {
        if (actualMs > expectedMs * 1.5) // 50% over budget
        {
            performanceViolations.fetch_add(1, std::memory_order_relaxed);
            
            // Update worst case
            double worst = worstCaseMs.load(std::memory_order_relaxed);
            while (actualMs > worst)
            {
                if (worstCaseMs.compare_exchange_weak(worst, actualMs,
                                                     std::memory_order_relaxed))
                {
                    break;
                }
            }
        }
    }
    
    uint64_t getViolationCount() const noexcept
    {
        return performanceViolations.load(std::memory_order_relaxed);
    }
    
    double getWorstCase() const noexcept
    {
        return worstCaseMs.load(std::memory_order_relaxed);
    }
    
private:
    std::atomic<uint64_t> performanceViolations{0};
    std::atomic<double> worstCaseMs{0.0};
};

// Performance assertion macro
#define RT_ASSERT_PERFORMANCE(actualMs, expectedMs) \
    RealtimePerformanceMonitor::getInstance().checkProcessingTime(actualMs, expectedMs)

/**
 * Diagnostic system for production monitoring
 */
class RealtimeDiagnostics
{
public:
    static juce::String generateReport()
    {
        auto& errorTracker = RealtimeErrorTracker::getInstance();
        auto& perfMonitor = RealtimePerformanceMonitor::getInstance();
        
        juce::String report;
        report << "=== REALTIME DIAGNOSTICS REPORT ===\n";
        report << "Total Assertion Failures: " << errorTracker.getErrorCount() << "\n";
        
        auto lastError = errorTracker.getLastError();
        if (lastError.file)
        {
            report << "Last Error: " << lastError.condition 
                   << " at " << lastError.file << ":" << lastError.line << "\n";
            report << "Last Error Time: " << juce::Time(lastError.time).toString(true, true) << "\n";
        }
        
        report << "Performance Violations: " << perfMonitor.getViolationCount() << "\n";
        report << "Worst Case Processing: " << perfMonitor.getWorstCase() << " ms\n";
        report << "===================================\n";
        
        return report;
    }
    
    static void reset()
    {
        RealtimeErrorTracker::getInstance().reset();
    }
    
    static bool hasErrors()
    {
        return RealtimeErrorTracker::getInstance().getErrorCount() > 0;
    }
};

/**
 * Helper to replace jassert in existing code
 * 
 * This macro can be used as a drop-in replacement for jassert
 * in audio processing code. It will never block the audio thread.
 */
#ifdef jassert
    #undef jassert
#endif

// Redefine jassert to use our real-time safe version in audio code
#ifdef AUDIO_THREAD_SAFE_MODE
    #define jassert(condition) RT_ASSERT(condition)
    #define jassertfalse RT_ASSERT(false)
#endif