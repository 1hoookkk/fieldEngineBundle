#pragma once
#include <JuceHeader.h>
#include "AtomicOscillator.h"
#include <atomic>
#include <array>
#include <cstring>

/**
 * Lock-free oscillator pool with O(1) allocation and deallocation
 * 
 * Key improvements:
 * - O(1) free list management (no linear search)
 * - Lock-free allocation/deallocation
 * - Cache-friendly memory layout
 * - Age-based replacement strategy
 * - SIMD-ready oscillator banks
 * 
 * @author SpectralCanvas Team
 * @version 2.0 - Production Optimized
 */

template<size_t MAX_OSCILLATORS = 1024>  // MetaSynth-compatible polyphony
class OptimizedOscillatorPool
{
public:
    // Statistics for monitoring
    struct Statistics
    {
        std::atomic<uint64_t> totalAllocations{0};
        std::atomic<uint64_t> totalDeallocations{0};
        std::atomic<uint64_t> failedAllocations{0};
        std::atomic<int> currentActive{0};
        std::atomic<int> peakActive{0};
        std::atomic<double> avgSearchTime{0.0};
    };
    
    OptimizedOscillatorPool()
    {
        // Initialize free list with all oscillator indices
        for (size_t i = 0; i < MAX_OSCILLATORS; ++i)
        {
            freeList[i] = static_cast<int>(i);
            oscillatorStates[i].store(OscillatorState::Free, std::memory_order_relaxed);
            lastUsedTime[i] = 0;
        }
        
        freeListHead.store(0, std::memory_order_relaxed);
        freeListTail.store(MAX_OSCILLATORS - 1, std::memory_order_relaxed);
        freeCount.store(MAX_OSCILLATORS, std::memory_order_relaxed);
    }
    
    /**
     * Allocate an oscillator - O(1) operation
     * Returns oscillator index or -1 if pool exhausted
     */
    int allocate() noexcept
    {
        // Track allocation time for statistics
        auto startTicks = juce::Time::getHighResolutionTicks();
        
        // Try to get a free oscillator from the head of the free list
        int head = freeListHead.load(std::memory_order_acquire);
        
        while (true)
        {
            // Check if free list is empty
            int count = freeCount.load(std::memory_order_acquire);
            if (count <= 0)
            {
                // Pool exhausted - try age-based replacement
                int victim = findOldestInactive();
                if (victim >= 0)
                {
                    updateAllocationStats(startTicks, false);
                    return victim;
                }
                
                stats.failedAllocations.fetch_add(1, std::memory_order_relaxed);
                return -1;
            }
            
            // Get the oscillator index from free list
            int nextHead = (head + 1) % MAX_OSCILLATORS;
            int oscillatorIndex = freeList[head];
            
            // Try to update the head atomically
            if (freeListHead.compare_exchange_weak(head, nextHead,
                                                  std::memory_order_release,
                                                  std::memory_order_acquire))
            {
                // Successfully got an oscillator
                freeCount.fetch_sub(1, std::memory_order_release);
                
                // Mark as allocated
                OscillatorState expected = OscillatorState::Free;
                oscillatorStates[oscillatorIndex].compare_exchange_strong(
                    expected, OscillatorState::Allocated,
                    std::memory_order_release);
                
                // Update statistics
                updateAllocationStats(startTicks, true);
                updateActiveCount(1);
                
                // Reset the oscillator
                oscillators[oscillatorIndex].reset();
                lastUsedTime[oscillatorIndex] = juce::Time::currentTimeMillis();
                
                return oscillatorIndex;
            }
            
            // CAS failed, retry with new head value
        }
    }
    
    /**
     * Deallocate an oscillator - O(1) operation
     */
    void deallocate(int index) noexcept
    {
        if (index < 0 || index >= static_cast<int>(MAX_OSCILLATORS))
            return;
            
        // Mark as free
        OscillatorState expected = OscillatorState::Allocated;
        if (!oscillatorStates[index].compare_exchange_strong(
                expected, OscillatorState::Free, std::memory_order_release))
        {
            // Already free or in wrong state
            return;
        }
        
        // Add back to free list at tail
        int tail = freeListTail.load(std::memory_order_acquire);
        
        while (true)
        {
            int nextTail = (tail + 1) % MAX_OSCILLATORS;
            
            if (freeListTail.compare_exchange_weak(tail, nextTail,
                                                  std::memory_order_release,
                                                  std::memory_order_acquire))
            {
                freeList[nextTail] = index;
                freeCount.fetch_add(1, std::memory_order_release);
                
                // Update statistics
                stats.totalDeallocations.fetch_add(1, std::memory_order_relaxed);
                updateActiveCount(-1);
                
                // Clear oscillator state
                oscillators[index].reset();
                
                break;
            }
        }
    }
    
    /**
     * Get oscillator by index (no bounds checking for performance)
     */
    AtomicOscillator& getOscillator(int index) noexcept
    {
        return oscillators[index];
    }
    
    /**
     * Get oscillator safely with bounds checking
     */
    AtomicOscillator* getOscillatorSafe(int index) noexcept
    {
        if (index >= 0 && index < static_cast<int>(MAX_OSCILLATORS))
        {
            return &oscillators[index];
        }
        return nullptr;
    }
    
    /**
     * Check if an oscillator is allocated
     */
    bool isAllocated(int index) const noexcept
    {
        if (index < 0 || index >= static_cast<int>(MAX_OSCILLATORS))
            return false;
            
        return oscillatorStates[index].load(std::memory_order_acquire) == OscillatorState::Allocated;
    }
    
    /**
     * Process all active oscillators
     */
    template<typename Processor>
    void processActive(Processor&& processor) noexcept
    {
        for (size_t i = 0; i < MAX_OSCILLATORS; ++i)
        {
            if (oscillatorStates[i].load(std::memory_order_acquire) == OscillatorState::Allocated)
            {
                processor(oscillators[i], static_cast<int>(i));
            }
        }
    }
    
    /**
     * Get pool statistics
     */
    Statistics getStatistics() const noexcept
    {
        return stats;
    }
    
    /**
     * Get current active count
     */
    int getActiveCount() const noexcept
    {
        return stats.currentActive.load(std::memory_order_relaxed);
    }
    
    /**
     * Get free count
     */
    int getFreeCount() const noexcept
    {
        return freeCount.load(std::memory_order_relaxed);
    }
    
    /**
     * Reset all oscillators and statistics
     */
    void reset() noexcept
    {
        // Reset all oscillators
        for (auto& osc : oscillators)
        {
            osc.reset();
        }
        
        // Reset free list
        for (size_t i = 0; i < MAX_OSCILLATORS; ++i)
        {
            freeList[i] = static_cast<int>(i);
            oscillatorStates[i].store(OscillatorState::Free, std::memory_order_relaxed);
            lastUsedTime[i] = 0;
        }
        
        freeListHead.store(0, std::memory_order_relaxed);
        freeListTail.store(MAX_OSCILLATORS - 1, std::memory_order_relaxed);
        freeCount.store(MAX_OSCILLATORS, std::memory_order_relaxed);
        
        // Reset statistics
        stats = Statistics();
    }
    
private:
    // Oscillator state enumeration
    enum class OscillatorState : uint8_t
    {
        Free = 0,
        Allocated = 1,
        Hibernating = 2  // For future use - oscillators that can be reclaimed
    };
    
    // Core data structures
    std::array<AtomicOscillator, MAX_OSCILLATORS> oscillators;
    std::array<std::atomic<OscillatorState>, MAX_OSCILLATORS> oscillatorStates;
    std::array<int64_t, MAX_OSCILLATORS> lastUsedTime;  // For age-based replacement
    
    // Lock-free circular free list
    std::array<int, MAX_OSCILLATORS> freeList;
    std::atomic<int> freeListHead{0};
    std::atomic<int> freeListTail{0};
    std::atomic<int> freeCount{0};
    
    // Statistics
    mutable Statistics stats;
    
    /**
     * Find oldest inactive oscillator for replacement (fallback strategy)
     */
    int findOldestInactive() noexcept
    {
        int64_t oldestTime = INT64_MAX;
        int oldestIndex = -1;
        int64_t currentTime = juce::Time::currentTimeMillis();
        
        for (size_t i = 0; i < MAX_OSCILLATORS; ++i)
        {
            if (oscillatorStates[i].load(std::memory_order_acquire) == OscillatorState::Allocated)
            {
                // Check if oscillator is inactive
                if (!oscillators[i].isActive())
                {
                    int64_t age = currentTime - lastUsedTime[i];
                    if (age > 100 && age < oldestTime)  // At least 100ms old
                    {
                        oldestTime = age;
                        oldestIndex = static_cast<int>(i);
                    }
                }
            }
        }
        
        if (oldestIndex >= 0)
        {
            // Reset and return the oldest inactive oscillator
            oscillators[oldestIndex].reset();
            lastUsedTime[oldestIndex] = currentTime;
        }
        
        return oldestIndex;
    }
    
    /**
     * Update allocation statistics
     */
    void updateAllocationStats(int64_t startTicks, bool fromFreeList) noexcept
    {
        stats.totalAllocations.fetch_add(1, std::memory_order_relaxed);
        
        // Calculate allocation time
        auto endTicks = juce::Time::getHighResolutionTicks();
        auto ticksPerSecond = juce::Time::getHighResolutionTicksPerSecond();
        double timeUs = ((endTicks - startTicks) * 1000000.0) / ticksPerSecond;
        
        // Update moving average
        double currentAvg = stats.avgSearchTime.load(std::memory_order_relaxed);
        double newAvg = currentAvg * 0.95 + timeUs * 0.05;
        stats.avgSearchTime.store(newAvg, std::memory_order_relaxed);
    }
    
    /**
     * Update active oscillator count
     */
    void updateActiveCount(int delta) noexcept
    {
        int newCount = stats.currentActive.fetch_add(delta, std::memory_order_relaxed) + delta;
        
        // Update peak if necessary
        int peak = stats.peakActive.load(std::memory_order_relaxed);
        while (newCount > peak)
        {
            if (stats.peakActive.compare_exchange_weak(peak, newCount,
                                                      std::memory_order_relaxed))
            {
                break;
            }
        }
    }
};

/**
 * SIMD-optimized oscillator pool for maximum performance
 */
template<size_t NUM_BANKS = 128, size_t BANK_SIZE = 4>
class SIMDOscillatorPool
{
public:
    static constexpr size_t TOTAL_OSCILLATORS = NUM_BANKS * BANK_SIZE;
    
    SIMDOscillatorPool()
    {
        // Initialize free bank list
        for (size_t i = 0; i < NUM_BANKS; ++i)
        {
            freeBankList[i] = static_cast<int>(i);
            bankStates[i].store(BankState::Free, std::memory_order_relaxed);
        }
        freeBankCount.store(NUM_BANKS, std::memory_order_relaxed);
    }
    
    /**
     * Allocate a bank of oscillators for SIMD processing
     */
    int allocateBank() noexcept
    {
        int count = freeBankCount.fetch_sub(1, std::memory_order_acquire) - 1;
        
        if (count >= 0)
        {
            int bankIndex = freeBankList[count];
            
            BankState expected = BankState::Free;
            if (bankStates[bankIndex].compare_exchange_strong(
                    expected, BankState::Allocated, std::memory_order_release))
            {
                banks[bankIndex].reset();
                return bankIndex;
            }
        }
        
        // Pool exhausted
        freeBankCount.fetch_add(1, std::memory_order_release);
        return -1;
    }
    
    /**
     * Deallocate a bank
     */
    void deallocateBank(int bankIndex) noexcept
    {
        if (bankIndex < 0 || bankIndex >= static_cast<int>(NUM_BANKS))
            return;
            
        BankState expected = BankState::Allocated;
        if (bankStates[bankIndex].compare_exchange_strong(
                expected, BankState::Free, std::memory_order_release))
        {
            int index = freeBankCount.fetch_add(1, std::memory_order_release);
            if (index < static_cast<int>(NUM_BANKS))
            {
                freeBankList[index] = bankIndex;
                banks[bankIndex].reset();
            }
        }
    }
    
    /**
     * Get oscillator bank for SIMD processing
     */
    AtomicOscillatorBank& getBank(int bankIndex) noexcept
    {
        return banks[bankIndex];
    }
    
    /**
     * Process all active banks with SIMD
     */
    void processAllBanks(juce::AudioBuffer<float>& buffer, float sampleRate) noexcept
    {
        for (size_t i = 0; i < NUM_BANKS; ++i)
        {
            if (bankStates[i].load(std::memory_order_acquire) == BankState::Allocated)
            {
                banks[i].processBlock(buffer, sampleRate);
            }
        }
    }
    
private:
    enum class BankState : uint8_t
    {
        Free = 0,
        Allocated = 1
    };
    
    std::array<AtomicOscillatorBank, NUM_BANKS> banks;
    std::array<std::atomic<BankState>, NUM_BANKS> bankStates;
    std::array<int, NUM_BANKS> freeBankList;
    std::atomic<int> freeBankCount{0};
};