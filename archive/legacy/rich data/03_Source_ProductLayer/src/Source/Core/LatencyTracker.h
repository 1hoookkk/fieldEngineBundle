#pragma once

#include <array>
#include <atomic>
#include <algorithm>
#include <cstdint>

/**
 * RT-safe latency measurement and statistics collection
 * Used to track paint→audio latency for Phase 2-3 validation
 * 
 * Thread safety:
 * - recordLatency(): Called from audio thread only
 * - getMedianLatencyMs/getP95LatencyMs(): Called from UI thread only
 * - No locks, no allocations in RT path
 */
class RTLatencyTracker
{
public:
    static constexpr size_t HISTORY_SIZE = 512;  // Must be power of 2
    static constexpr size_t HISTORY_MASK = HISTORY_SIZE - 1;
    
    RTLatencyTracker() noexcept
        : writeIndex_(0)
        , sampleCount_(0)
    {
        latencyMicros_.fill(0);
    }
    
    /**
     * Record a latency measurement (RT-safe, audio thread only)
     * @param latencyMicros Measured latency in microseconds
     */
    void recordLatency(uint32_t latencyMicros) noexcept
    {
        const size_t index = writeIndex_.fetch_add(1, std::memory_order_relaxed) & HISTORY_MASK;
        latencyMicros_[index] = latencyMicros;
        
        // Track total samples (saturate at max to avoid overflow)
        size_t current = sampleCount_.load(std::memory_order_relaxed);
        if (current < HISTORY_SIZE) {
            sampleCount_.store(current + 1, std::memory_order_relaxed);
        }
    }
    
    /**
     * Get median latency in milliseconds (UI thread only)
     * @return Median latency in ms, 0.0f if no samples
     */
    float getMedianLatencyMs() const noexcept
    {
        return getPercentileLatencyMs(0.5f);
    }
    
    /**
     * Get 95th percentile latency in milliseconds (UI thread only)
     * @return P95 latency in ms, 0.0f if no samples
     */
    float getP95LatencyMs() const noexcept
    {
        return getPercentileLatencyMs(0.95f);
    }
    
    /**
     * Get number of latency samples recorded
     */
    size_t getSampleCount() const noexcept
    {
        return sampleCount_.load(std::memory_order_relaxed);
    }
    
    /**
     * Reset all statistics (UI thread only)
     */
    void reset() noexcept
    {
        writeIndex_.store(0, std::memory_order_relaxed);
        sampleCount_.store(0, std::memory_order_relaxed);
        latencyMicros_.fill(0);
    }

private:
    /**
     * Calculate percentile latency (UI thread only)
     * @param percentile Value from 0.0 to 1.0
     * @return Latency in milliseconds
     */
    float getPercentileLatencyMs(float percentile) const noexcept
    {
        const size_t numSamples = sampleCount_.load(std::memory_order_relaxed);
        if (numSamples == 0) return 0.0f;
        
        // Create sorted copy of valid samples
        const size_t samplesInUse = std::min(numSamples, HISTORY_SIZE);
        std::array<uint32_t, HISTORY_SIZE> sortedSamples;
        
        // Copy current samples (may have some wraparound, but that's OK for stats)
        for (size_t i = 0; i < samplesInUse; ++i) {
            sortedSamples[i] = latencyMicros_[i];
        }
        
        // Partial sort to find percentile
        std::partial_sort(sortedSamples.begin(), 
                         sortedSamples.begin() + samplesInUse,
                         sortedSamples.begin() + samplesInUse);
        
        // Calculate percentile index
        size_t percentileIndex = static_cast<size_t>((samplesInUse - 1) * percentile);
        percentileIndex = std::min(percentileIndex, samplesInUse - 1);
        
        // Convert microseconds to milliseconds
        return sortedSamples[percentileIndex] / 1000.0f;
    }
    
    alignas(64) std::array<uint32_t, HISTORY_SIZE> latencyMicros_;
    std::atomic<size_t> writeIndex_;
    std::atomic<size_t> sampleCount_;
};