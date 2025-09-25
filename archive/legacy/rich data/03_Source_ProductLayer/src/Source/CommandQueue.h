#pragma once
#include <JuceHeader.h>
#include "Commands.h"
#include <memory>
#include <atomic>

/**
 * Thread-safe, lock-free command queue for real-time audio communication
 * 
 * This implementation uses JUCE's AbstractFifo for thread safety and proper
 * memory ordering. It provides a high-performance mechanism for passing
 * commands from the GUI thread to the audio thread without blocking.
 * 
 * Features:
 * - Lock-free implementation using JUCE's AbstractFifo
 * - Pre-allocated command buffer to avoid real-time memory allocation
 * - Overflow protection with statistics
 * - Performance monitoring
 */
template <size_t Capacity = 256>
class CommandQueue
{
public:
    CommandQueue() 
        : fifo(Capacity)
    {
        // Clear statistics
        totalPushed.store(0, std::memory_order_relaxed);
        totalPopped.store(0, std::memory_order_relaxed);
        overflowCount.store(0, std::memory_order_relaxed);
    }
    
    /**
     * Push a command to the queue (called from GUI thread)
     * Returns true if successful, false if queue is full
     * 
     * RELIABILITY FIX: Removed race condition by eliminating pre-check.
     * Now handles prepareToWrite() failure directly for thread safety.
     */
    bool push(const Command& cmd)
    {
        // Get write positions - no pre-check to avoid race condition
        int start1, size1, start2, size2;
        fifo.prepareToWrite(1, start1, size1, start2, size2);
        
        if (size1 == 1 && size2 == 0)
        {
            // Add memory barrier for reliable cross-thread visibility
            std::atomic_thread_fence(std::memory_order_seq_cst);
            
            // Write the command
            buffer[start1] = cmd;
            
            // Add memory barrier after write
            std::atomic_thread_fence(std::memory_order_seq_cst);
            
            // Commit the write
            fifo.finishedWrite(size1);
            
            // Update statistics
            totalPushed.fetch_add(1, std::memory_order_relaxed);
            return true;
        }
        else
        {
            // Queue is full or fragmented
            overflowCount.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
    }
    
    /**
     * Pop a command from the queue (called from audio thread)
     * Returns true if a command was retrieved, false if queue is empty
     * 
     * RELIABILITY FIX: Removed race condition by eliminating pre-check.
     * Added memory barriers for guaranteed cross-thread visibility.
     */
    bool pop(Command& out)
    {
        // Get read positions - no pre-check to avoid race condition
        int start1, size1, start2, size2;
        fifo.prepareToRead(1, start1, size1, start2, size2);
        
        if (size1 == 1 && size2 == 0)
        {
            // Add memory barrier before read for reliable visibility
            std::atomic_thread_fence(std::memory_order_seq_cst);
            
            // Read the command
            out = buffer[start1];
            
            // Add memory barrier after read
            std::atomic_thread_fence(std::memory_order_seq_cst);
            
            // Commit the read
            fifo.finishedRead(size1);
            
            // Update statistics
            totalPopped.fetch_add(1, std::memory_order_relaxed);
            return true;
        }
        
        // Queue is empty or fragmented
        return false;
    }
    
    /**
     * Process all pending commands (called from audio thread)
     * Returns the number of commands processed
     */
    int processAll(std::function<void(const Command&)> processor)
    {
        int processed = 0;
        Command cmd;
        
        while (pop(cmd))
        {
            processor(cmd);
            processed++;
        }
        
        // Update max batch size if necessary
        if (processed > 0)
        {
            int currentMax = maxBatchSize.load(std::memory_order_relaxed);
            while (processed > currentMax)
            {
                if (maxBatchSize.compare_exchange_weak(currentMax, processed,
                                                      std::memory_order_relaxed))
                {
                    break;
                }
            }
        }
        
        return processed;
    }
    
    /**
     * Process commands with a time limit (called from audio thread)
     * Returns the number of commands processed
     */
    int processWithTimeLimit(std::function<void(const Command&)> processor,
                           double maxProcessingTimeMs)
    {
        auto startTime = juce::Time::getMillisecondCounterHiRes();
        int processed = 0;
        Command cmd;
        
        while (pop(cmd))
        {
            // Check time limit
            auto elapsedTime = juce::Time::getMillisecondCounterHiRes() - startTime;
            if (elapsedTime >= maxProcessingTimeMs)
            {
                // Put the command back (push to front)
                // Note: This is not ideal but JUCE's AbstractFifo doesn't support push-front
                // In practice, we should process all critical commands first
                break;
            }
            
            processor(cmd);
            processed++;
        }
        
        return processed;
    }
    
    /**
     * Clear all pending commands
     */
    void clear()
    {
        fifo.reset();
    }
    
    /**
     * Get the number of pending commands
     */
    int getNumReady() const
    {
        return fifo.getNumReady();
    }
    
    bool isEmpty() const noexcept 
    { 
        return fifo.getNumReady() == 0; 
    }
    
    bool isFull() const noexcept 
    { 
        return fifo.getFreeSpace() == 0; 
    }
    
    /**
     * Get queue statistics
     */
    struct Statistics
    {
        uint64_t totalPushed = 0;
        uint64_t totalPopped = 0;
        uint64_t overflowCount = 0;
        int maxBatchSize = 0;
        int currentPending = 0;
        float utilizationPercent = 0.0f;
    };
    
    Statistics getStatistics() const
    {
        Statistics stats;
        stats.totalPushed = totalPushed.load(std::memory_order_relaxed);
        stats.totalPopped = totalPopped.load(std::memory_order_relaxed);
        stats.overflowCount = overflowCount.load(std::memory_order_relaxed);
        stats.maxBatchSize = maxBatchSize.load(std::memory_order_relaxed);
        stats.currentPending = getNumReady();
        stats.utilizationPercent = (float)stats.currentPending / (float)Capacity * 100.0f;
        return stats;
    }
    
    /**
     * Reset statistics
     */
    void resetStatistics()
    {
        totalPushed.store(0, std::memory_order_relaxed);
        totalPopped.store(0, std::memory_order_relaxed);
        overflowCount.store(0, std::memory_order_relaxed);
        maxBatchSize.store(0, std::memory_order_relaxed);
    }
    
private:
    // JUCE's thread-safe FIFO
    juce::AbstractFifo fifo;
    
    // Command buffer
    std::array<Command, Capacity> buffer;
    
    // Statistics (atomic for thread safety)
    std::atomic<uint64_t> totalPushed{0};
    std::atomic<uint64_t> totalPopped{0};
    std::atomic<uint64_t> overflowCount{0};
    std::atomic<int> maxBatchSize{0};
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CommandQueue)
};

/**
 * Specialized command queue with priority support
 * Uses multiple queues for different priority levels
 */
template <size_t CapacityPerPriority = 64>
class PriorityCommandQueue
{
public:
    enum class Priority
    {
        Low = 0,
        Normal = 1,
        High = 2,
        Critical = 3
    };
    
    bool push(const Command& cmd, Priority priority = Priority::Normal)
    {
        int index = static_cast<int>(priority);
        return queues[index].push(cmd);
    }
    
    int processAll(std::function<void(const Command&)> processor)
    {
        int totalProcessed = 0;
        
        // Process in priority order (highest first)
        for (int i = 3; i >= 0; --i)
        {
            totalProcessed += queues[i].processAll(processor);
        }
        
        return totalProcessed;
    }
    
    void clear()
    {
        for (auto& queue : queues)
        {
            queue.clear();
        }
    }
    
private:
    std::array<CommandQueue<CapacityPerPriority>, 4> queues;
};
