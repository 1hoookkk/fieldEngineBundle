#pragma once

#include <juce_core/juce_core.h>
#include <string>
#include <sstream>

/**
 * @file DiagnosticLogger.h
 * @brief Structured logging system for sample loading diagnostics
 * 
 * Provides category-based logging with levels and compile-time verbosity control.
 * All logs include timestamp, file:line info, and structured formatting.
 */

namespace DiagnosticLogger
{
    enum class Category : uint8_t
    {
        LOADER = 0,
        DECODER,
        RENDERER, 
        UI
    };

    enum class Level : uint8_t
    {
        VERBOSE = 0,
        INFO,
        WARN,
        ERROR
    };

    /**
     * Core logging function - formats and outputs to JUCE Logger
     * @param category Log category 
     * @param level Log level
     * @param file Source file (__FILE__)
     * @param line Source line (__LINE__)
     * @param message Formatted message
     */
    void logMessage(Category category, Level level, const char* file, int line, const juce::String& message);

    /**
     * Get category name for display
     */
    const char* getCategoryName(Category category) noexcept;

    /**
     * Get level name for display
     */
    const char* getLevelName(Level level) noexcept;

    /**
     * Check if level should be logged in current build
     */
    bool shouldLog(Level level) noexcept;

    /**
     * Format helper for consistent message building
     */
    template<typename... Args>
    juce::String format(const char* fmt, Args&&... args)
    {
        return juce::String::formatted(fmt, std::forward<Args>(args)...);
    }
}

// Convenience macros for structured logging
#define DIAG_LOG(cat, level, msg) \
    do { \
        if (DiagnosticLogger::shouldLog(DiagnosticLogger::Level::level)) { \
            DiagnosticLogger::logMessage(DiagnosticLogger::Category::cat, \
                                       DiagnosticLogger::Level::level, \
                                       __FILE__, __LINE__, msg); \
        } \
    } while(0)

#define LOGV(cat, fmt, ...) DIAG_LOG(cat, VERBOSE, DiagnosticLogger::format(fmt, ##__VA_ARGS__))
#define LOGI(cat, fmt, ...) DIAG_LOG(cat, INFO, DiagnosticLogger::format(fmt, ##__VA_ARGS__))
#define LOGW(cat, fmt, ...) DIAG_LOG(cat, WARN, DiagnosticLogger::format(fmt, ##__VA_ARGS__))
#define LOGE(cat, fmt, ...) DIAG_LOG(cat, ERROR, DiagnosticLogger::format(fmt, ##__VA_ARGS__))

// Simple message versions
#define LOGV_MSG(cat, msg) DIAG_LOG(cat, VERBOSE, juce::String(msg))
#define LOGI_MSG(cat, msg) DIAG_LOG(cat, INFO, juce::String(msg))
#define LOGW_MSG(cat, msg) DIAG_LOG(cat, WARN, juce::String(msg))
#define LOGE_MSG(cat, msg) DIAG_LOG(cat, ERROR, juce::String(msg))