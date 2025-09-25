#pragma once
#include <ctime>

/**
 * Platform-specific time utilities to handle deprecated functions.
 * Provides safe wrappers for localtime and other platform-specific time functions.
 */
namespace PlatformTimeUtils {
    
    /**
     * Safe wrapper for localtime that works on both Windows and POSIX.
     * Uses localtime_s on Windows, localtime_r on POSIX.
     * 
     * @param result Pointer to tm struct to fill
     * @param timer Pointer to time_t value to convert
     * @return true on success, false on error
     */
    inline bool getLocalTime(std::tm* result, const std::time_t* timer) noexcept {
        if (!result || !timer) return false;
        
    #ifdef _WIN32
        // Windows: use localtime_s (secure version)
        return localtime_s(result, timer) == 0;
    #else
        // POSIX: use localtime_r (reentrant version)
        return localtime_r(timer, result) != nullptr;
    #endif
    }
    
    /**
     * Get current local time safely.
     * @param result Pointer to tm struct to fill with current local time
     * @return true on success, false on error
     */
    inline bool getCurrentLocalTime(std::tm* result) noexcept {
        std::time_t now = std::time(nullptr);
        return getLocalTime(result, &now);
    }
    
} // namespace PlatformTimeUtils