#pragma once
#include <atomic>
#include <cstdint>

/**
 * @brief Lock-free Single Producer Single Consumer (SPSC) queue for paint events
 * 
 * RT-safe queue for passing paint data from UI thread to audio thread.
 * Based on the user's recommended implementation - power of 2 capacity for fast modulo.
 */

// Paint event stroke lifecycle flags
enum : uint32_t { 
    kStrokeStart = 1u << 0, 
    kStrokeMove  = 1u << 1, 
    kStrokeEnd   = 1u << 2 
};

struct PaintEvent 
{
    float nx;        // 0..1 normalized X (canvas space)
    float ny;        // 0..1 normalized Y
    float pressure;  // 0..1
    uint32_t flags;  // kStrokeStart/Move/End (exactly one)
    uint32_t color;  // optional: packed RGBA or brush ID
    
    PaintEvent() = default;
    PaintEvent(float x, float y, float p, uint32_t f = 0, uint32_t c = 0) 
        : nx(x), ny(y), pressure(p), flags(f), color(c) {}
};

template <typename T, size_t Capacity>
struct PaintQueue 
{
    static_assert((Capacity & (Capacity-1)) == 0, "Capacity must be power of 2");
    static_assert(Capacity >= 16, "Minimum capacity of 16 for reasonable buffering");
    static_assert(Capacity <= 16384, "Maximum capacity of 16384 to prevent excessive memory usage");
    
    T ring[Capacity];
    std::atomic<size_t> writeIndex{0};
    std::atomic<size_t> readIndex{0};
    
    /**
     * @brief Push an event to the queue (called from UI thread)
     * @param value Event to push
     * @return true if successful, false if queue is full
     */
    bool push(const T& value) noexcept
    {
        const size_t currentWrite = writeIndex.load(std::memory_order_relaxed);
        const size_t currentRead = readIndex.load(std::memory_order_acquire);
        
        // Check if queue is full (next write position would equal read position)
        if (((currentWrite + 1) & (Capacity - 1)) == (currentRead & (Capacity - 1))) {
            return false; // Queue is full
        }
        
        // Store the value
        ring[currentWrite & (Capacity - 1)] = value;
        
        // Publish the write (release semantics ensure value is written before index update)
        writeIndex.store(currentWrite + 1, std::memory_order_release);
        
        return true;
    }
    
    /**
     * @brief Pop an event from the queue (called from audio thread)
     * @param out Reference to store the popped event
     * @return true if successful, false if queue is empty
     */
    bool pop(T& out) noexcept
    {
        const size_t currentRead = readIndex.load(std::memory_order_relaxed);
        const size_t currentWrite = writeIndex.load(std::memory_order_acquire);
        
        // Check if queue is empty
        if (currentRead == currentWrite) {
            return false; // Queue is empty
        }
        
        // Read the value
        out = ring[currentRead & (Capacity - 1)];
        
        // Publish the read (release semantics for proper synchronization)
        readIndex.store(currentRead + 1, std::memory_order_release);
        
        return true;
    }
    
    /**
     * @brief Get approximate number of items in queue (for debugging)
     * @note This is approximate due to concurrent access, use only for monitoring
     */
    size_t approxSize() const noexcept
    {
        const size_t currentWrite = writeIndex.load(std::memory_order_relaxed);
        const size_t currentRead = readIndex.load(std::memory_order_relaxed);
        return (currentWrite - currentRead) & (Capacity - 1);
    }
    
    /**
     * @brief Check if queue is approximately empty
     */
    bool empty() const noexcept
    {
        return readIndex.load(std::memory_order_relaxed) == 
               writeIndex.load(std::memory_order_relaxed);
    }
    
    /**
     * @brief Get capacity of the queue
     */
    static constexpr size_t capacity() noexcept
    {
        return Capacity;
    }
    
    /**
     * @brief Clear the queue (should only be called when no concurrent access)
     */
    void clear() noexcept
    {
        writeIndex.store(0, std::memory_order_relaxed);
        readIndex.store(0, std::memory_order_relaxed);
    }
};

// Type alias for the specific paint queue we'll use
// 4096 events should provide plenty of buffering for paint strokes
using SpectralPaintQueue = PaintQueue<PaintEvent, 4096>;