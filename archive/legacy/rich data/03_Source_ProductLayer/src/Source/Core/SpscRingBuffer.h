#pragma once
#include <atomic>
#include <array>
#include <cstddef>
#include <optional>
#include <type_traits>

/**
 * Lock-free single-producer/single-consumer ring buffer.
 * Capacity must be > 1; queue holds at most (Capacity-1) items.
 * 
 * Thread-safe communication between UI and audio threads.
 * No allocations or locks in operations.
 */
template <typename T, std::size_t Capacity>
class SpscRingBuffer {
    static_assert(Capacity > 1, "Capacity must be > 1");
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable for lock-free SPSC");

public:
    SpscRingBuffer() = default;

    // Producer thread (UI/Message thread)
    bool push(const T& item) noexcept {
        const auto next = (head_ + 1) % Capacity;
        if (next == tail_.load(std::memory_order_acquire))
            return false; // full
        buffer_[head_] = item;
        head_ = next;
        return true;
    }

    // Consumer thread (audio thread) - returns optional for compatibility
    std::optional<T> pop() noexcept {
        const auto t = tail_.load(std::memory_order_acquire);
        if (t == head_) 
            return std::nullopt; // empty
        T item = buffer_[t];
        tail_.store((t + 1) % Capacity, std::memory_order_release);
        return item;
    }

    // Alternative pop method with reference parameter
    bool pop(T& out) noexcept {
        const auto t = tail_.load(std::memory_order_acquire);
        if (t == head_) return false; // empty
        out = buffer_[t];
        tail_.store((t + 1) % Capacity, std::memory_order_release);
        return true;
    }

    std::size_t size() const noexcept {
        const auto h = head_;
        const auto t = tail_.load(std::memory_order_acquire);
        return (h + Capacity - t) % Capacity;
    }

    std::size_t freeSpace() const noexcept {
        return Capacity - 1 - size();
    }

    // API compatibility methods
    void clear() noexcept {
        head_ = 0;
        tail_.store(0, std::memory_order_release);
    }

    bool hasSpaceAvailable() const noexcept { return freeSpace() > 0; }
    bool hasDataAvailable() const noexcept { return size() > 0; }
    std::size_t getApproxQueueDepth() const noexcept { return size(); }
    std::size_t getDropCount() const noexcept { return 0; } // Not tracked in this implementation

private:
    std::array<T, Capacity> buffer_{};
    std::size_t head_ = 0;                 // producer-only
    std::atomic<std::size_t> tail_{0};     // consumer-updated
};