#include "DiagnosticLogger.h"
#include "PlatformTimeUtils.h"
#include <chrono>
#include <iomanip>

namespace DiagnosticLogger
{
    void logMessage(Category category, Level level, const char* file, int line, const juce::String& message)
    {
        if (!shouldLog(level))
            return;

        // Extract filename from full path
        const char* filename = file;
        if (const char* lastSlash = strrchr(file, '/'))
            filename = lastSlash + 1;
        if (const char* lastBackslash = strrchr(filename, '\\'))
            filename = lastBackslash + 1;

        // Get current timestamp
        auto now = std::chrono::system_clock::now();
        auto timeT = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

        // Format: [HH:MM:SS.mmm] [CAT] [LEVEL] filename:line - message
        std::stringstream ss;
        std::tm localTime;
        if (PlatformTimeUtils::getLocalTime(&localTime, &timeT))
        {
            ss << "[" << std::put_time(&localTime, "%H:%M:%S") 
               << "." << std::setfill('0') << std::setw(3) << ms.count() << "] ";
        }
        else
        {
            // Fallback if localtime conversion fails
            ss << "[??:??:??.???] ";
        }
        
        ss << "[" << getCategoryName(category) << "] "
           << "[" << getLevelName(level) << "] "
           << filename << ":" << line << " - " << message.toStdString();

        // Output to JUCE logger
        juce::Logger::writeToLog(juce::String(ss.str()));
    }

    const char* getCategoryName(Category category) noexcept
    {
        switch (category)
        {
            case Category::LOADER:   return "LOAD";
            case Category::DECODER:  return "DEC ";
            case Category::RENDERER: return "REND";
            case Category::UI:       return "UI  ";
            default:                 return "UNKN";
        }
    }

    const char* getLevelName(Level level) noexcept
    {
        switch (level)
        {
            case Level::VERBOSE: return "VERB";
            case Level::INFO:    return "INFO";
            case Level::WARN:    return "WARN";
            case Level::ERROR:   return "ERR ";
            default:             return "UNKN";
        }
    }

    bool shouldLog(Level level) noexcept
    {
#ifdef JUCE_DEBUG
        // Debug builds: log everything including VERBOSE
        return true;
#else
        // Release builds: skip VERBOSE unless explicitly enabled
    #ifdef ENABLE_VERBOSE_LOGS
        return true;
    #else
        return level >= Level::INFO;
    #endif
#endif
    }
}