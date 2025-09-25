#pragma once

#include <atomic>
#include <cstdint>
#include <juce_core/juce_core.h>

/**
 * Lock-free Single Producer Single Consumer (SPSC) queue for paint events
 * 
 * RT-safe queue for passing paint data from UI thread to audio thread.
 * Battle-tested implementation from ARTEFACT_Production.
 * Power of 2 capacity for fast modulo operations.
 */

// Paint event stroke lifecycle flags
enum : uint32_t 
{ 
    kStrokeStart = 1u << 0, 
    kStrokeMove  = 1u << 1, 
    kStrokeEnd   = 1u << 2 
};

struct PaintEvent 
{
    float nx;        // 0..1 normalized X (canvas space)
    float ny;        // 0..1 normalized Y  
    float pressure;  // 0..1
    float velocity;  // 0..1 stroke velocity
    uint32_t flags;  // kStrokeStart/Move/End (exactly one)
    uint32_t color;  // Packed RGBA or brush ID
    uint32_t binIndex; // Frequency bin for quantized modes
    double timestamp; // Sample-accurate timing
    
    PaintEvent() = default;
    PaintEvent(float x, float y, float p, uint32_t f = 0) 
        : nx(x), ny(y), pressure(p), velocity(0.0f), 
          flags(f), color(0xFFFFFFFF), binIndex(0), timestamp(0.0) {}
};

#if _MSC_VER
  #pragma warning(push)
  #pragma warning(disable:4324)
#endif

template <typename T, size_t Capacity>
class PaintQueue 
{
public:
    static_assert((Capacity & (Capacity-1)) == 0, "Capacity must be power of 2");
    static_assert(Capacity >= 16, "Minimum capacity of 16 for reasonable buffering");
    static_assert(Capacity <= 16384, "Maximum capacity of 16384 to prevent excessive memory usage");
    
    PaintQueue() noexcept = default;
    ~PaintQueue() noexcept = default;
    
    // Delete copy operations (lock-free structures must not be copied)
    PaintQueue(const PaintQueue&) = delete;
    PaintQueue& operator=(const PaintQueue&) = delete;
    
    /**
     * Push an event to the queue (called from UI thread)
     * @param value Event to push
     * @return true if successful, false if queue is full
     */
    bool push(const T& value) noexcept
    {
        const size_t currentWrite = writeIndex.load(std::memory_order_relaxed);
        const size_t currentRead = readIndex.load(std::memory_order_acquire);

        // Check if queue is full using absolute counters
        // Full when available slots < 1: (write - read) >= (Capacity - 1)
        if ((currentWrite - currentRead) >= (Capacity - 1)) {
            return false; // Queue is full
        }

        // Store the value
        ring[currentWrite & (Capacity - 1)] = value;

        // Publish the write (release semantics ensure value is written before index update)
        writeIndex.store(currentWrite + 1, std::memory_order_release);

        return true;
    }
    
    /**
     * Pop an event from the queue (called from audio thread)
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
     * Try to pop an event (returns optional)
     */
    std::optional<T> tryPop() noexcept
    {
        T value;
        if (pop(value)) {
            return value;
        }
        return std::nullopt;
    }
    
    /**
     * Get approximate number of items in queue (for debugging)
     * @note This is approximate due to concurrent access, use only for monitoring
     */
    size_t approxSize() const noexcept
    {
        const size_t currentWrite = writeIndex.load(std::memory_order_relaxed);
        const size_t currentRead = readIndex.load(std::memory_order_relaxed);
        return (currentWrite - currentRead);
    }
    
    /**
     * Check if queue is approximately empty
     */
    bool empty() const noexcept
    {
        return readIndex.load(std::memory_order_relaxed) == 
               writeIndex.load(std::memory_order_relaxed);
    }
    
    /**
     * Check if queue has space available
     */
    bool hasSpace() const noexcept
    {
        const size_t currentWrite = writeIndex.load(std::memory_order_relaxed);
        const size_t currentRead = readIndex.load(std::memory_order_acquire);
        return (currentWrite - currentRead) < (Capacity - 1);
    }
    
    /**
     * Get capacity of the queue
     */
    static constexpr size_t capacity() noexcept
    {
        return Capacity;
    }
    
    /**
     * Clear the queue (should only be called when no concurrent access)
     */
    void clear() noexcept
    {
        writeIndex.store(0, std::memory_order_relaxed);
        readIndex.store(0, std::memory_order_relaxed);
    }
    
private:
    alignas(64) T ring[Capacity] = {}; // Cache-line aligned ring buffer
    alignas(64) std::atomic<size_t> writeIndex{0}; // Producer cache line
    alignas(64) std::atomic<size_t> readIndex{0};  // Consumer cache line
};

#if _MSC_VER
  #pragma warning(pop)
#endif

// Type aliases for specific paint queues
using SpectralPaintQueue = PaintQueue<PaintEvent, 4096>;    // UI → Audio paint events
using GestureQueue = PaintQueue<PaintEvent, 256>;           // Fast gesture queue