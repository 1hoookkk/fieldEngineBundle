#pragma once
#include <cstdint>
#include <chrono>

/**
 * Cross-platform time utilities for high-precision timing.
 * Provides microsecond-precision timestamps for audio/UI latency measurement.
 */
namespace TimeUtils {
    
    /**
     * Get current time in microseconds since steady_clock epoch.
     * Thread-safe, monotonic, suitable for latency measurements.
     */
    inline uint64_t getCurrentTimeUs() noexcept {
        auto now = std::chrono::steady_clock::now();
        auto duration = now.time_since_epoch();
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(duration).count());
    }
    
    /**
     * Get current time in nanoseconds for high-precision measurements.
     */
    inline uint64_t getCurrentTimeNs() noexcept {
        auto now = std::chrono::steady_clock::now();
        auto duration = now.time_since_epoch();
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count());
    }
    
    /**
     * Calculate elapsed microseconds between two timepoints.
     */
    inline uint64_t elapsedUs(uint64_t startTimeUs) noexcept {
        return getCurrentTimeUs() - startTimeUs;
    }
    
} // namespace TimeUtils