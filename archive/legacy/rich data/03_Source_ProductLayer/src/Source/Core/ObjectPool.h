#pragma once

#include <JuceHeader.h>
#include <vector>
#include <atomic>
#include <memory>

/**
 * Real-time safe object pool for eliminating dynamic memory allocation
 * 
 * This template class provides a lock-free, pre-allocated pool of objects
 * that can be safely used in real-time audio contexts without triggering
 * memory allocation or deallocation.
 * 
 * Features:
 * - Lock-free operation using atomic indices
 * - Pre-allocated storage initialized at startup
 * - Automatic object initialization/reset on acquire/release
 * - Thread-safe acquire/release operations
 * - Statistics for pool utilization monitoring
 */
template<typename T, size_t PoolSize = 64>
class ObjectPool
{
public:
    /**
     * RAII wrapper for automatic object return to pool
     */
    class PooledObject
    {
    public:
        explicit PooledObject(ObjectPool* pool, T* obj) 
            : pool_(pool), object_(obj) {}
        
        ~PooledObject() 
        {
            if (pool_ && object_)
                pool_->release(object_);
        }
        
        // Move semantics only - no copying
        PooledObject(const PooledObject&) = delete;
        PooledObject& operator=(const PooledObject&) = delete;
        
        PooledObject(PooledObject&& other) noexcept
            : pool_(other.pool_), object_(other.object_)
        {
            other.pool_ = nullptr;
            other.object_ = nullptr;
        }
        
        PooledObject& operator=(PooledObject&& other) noexcept
        {
            if (this != &other)
            {
                if (pool_ && object_)
                    pool_->release(object_);
                    
                pool_ = other.pool_;
                object_ = other.object_;
                other.pool_ = nullptr;
                other.object_ = nullptr;
            }
            return *this;
        }
        
        T* get() const { return object_; }
        T* operator->() const { return object_; }
        T& operator*() const { return *object_; }
        
        bool isValid() const { return object_ != nullptr; }
        
        // Release early (before destructor)
        void reset()
        {
            if (pool_ && object_)
            {
                pool_->release(object_);
                pool_ = nullptr;
                object_ = nullptr;
            }
        }
        
    private:
        ObjectPool* pool_;
        T* object_;
    };
    
    ObjectPool()
    {
        // Pre-allocate all objects
        objects_.reserve(PoolSize);
        for (size_t i = 0; i < PoolSize; ++i)
        {
            objects_.emplace_back(std::make_unique<T>());
            freeIndices_[i] = static_cast<int>(i);
        }
        
        nextFreeIndex_.store(PoolSize - 1, std::memory_order_relaxed);
        totalAcquired_.store(0, std::memory_order_relaxed);
        totalReleased_.store(0, std::memory_order_relaxed);
        peakUsage_.store(0, std::memory_order_relaxed);
        
        DBG("ObjectPool<" << typeid(T).name() << "> initialized with " << PoolSize << " objects");
    }
    
    ~ObjectPool()
    {
        // Objects are automatically cleaned up by unique_ptr
    }
    
    /**
     * Acquire an object from the pool
     * Returns nullptr if pool is exhausted
     * Thread-safe
     */
    T* acquire()
    {
        // Atomically decrement the free index to "claim" an object
        int currentIndex = nextFreeIndex_.fetch_sub(1, std::memory_order_acq_rel);
        
        if (currentIndex < 0)
        {
            // Pool exhausted - restore the index and return nullptr
            nextFreeIndex_.fetch_add(1, std::memory_order_acq_rel);
            return nullptr;
        }
        
        // Get the object at the claimed index
        int objectIndex = freeIndices_[currentIndex];
        T* object = objects_[objectIndex].get();
        
        // Reset object to default state
        *object = T{};
        
        // Update statistics
        totalAcquired_.fetch_add(1, std::memory_order_relaxed);
        updatePeakUsage();
        
        return object;
    }
    
    /**
     * Acquire an object with RAII wrapper
     * Returns an invalid PooledObject if pool is exhausted
     * Thread-safe
     */
    PooledObject acquireScoped()
    {
        T* object = acquire();
        return PooledObject(this, object);
    }
    
    /**
     * Release an object back to the pool
     * Thread-safe
     */
    void release(T* object)
    {
        if (!object)
            return;
        
        // Find the index of this object
        int objectIndex = -1;
        for (size_t i = 0; i < PoolSize; ++i)
        {
            if (objects_[i].get() == object)
            {
                objectIndex = static_cast<int>(i);
                break;
            }
        }
        
        if (objectIndex == -1)
        {
            // Object doesn't belong to this pool - this is a bug
            // AUDIO THREAD SAFETY: Use DBG instead of jassert to avoid blocking audio
            DBG("ObjectPool::release() - Object doesn't belong to this pool (memory corruption or double-release)");
            return;
        }
        
        // Reset object to clean state
        *object = T{};
        
        // Atomically increment the free index and store the object index
        int nextIndex = nextFreeIndex_.fetch_add(1, std::memory_order_acq_rel) + 1;
        
        if (nextIndex >= static_cast<int>(PoolSize))
        {
            // This shouldn't happen - indicates double-release or other bug
            // AUDIO THREAD SAFETY: Use DBG instead of jassert to avoid blocking audio
            DBG("ObjectPool::release() - Pool overflow detected (nextIndex=" << nextIndex << ", PoolSize=" << PoolSize << ")");
            nextFreeIndex_.fetch_sub(1, std::memory_order_acq_rel);
            return;
        }
        
        freeIndices_[nextIndex] = objectIndex;
        
        // Update statistics
        totalReleased_.fetch_add(1, std::memory_order_relaxed);
    }
    
    /**
     * Get current pool statistics
     */
    struct Statistics
    {
        size_t totalObjects = 0;
        size_t availableObjects = 0;
        size_t usedObjects = 0;
        uint64_t totalAcquired = 0;
        uint64_t totalReleased = 0;
        size_t peakUsage = 0;
        float utilizationPercent = 0.0f;
    };
    
    Statistics getStatistics() const
    {
        Statistics stats;
        stats.totalObjects = PoolSize;
        
        int freeCount = nextFreeIndex_.load(std::memory_order_relaxed) + 1;
        stats.availableObjects = std::max(0, freeCount);
        stats.usedObjects = PoolSize - stats.availableObjects;
        
        stats.totalAcquired = totalAcquired_.load(std::memory_order_relaxed);
        stats.totalReleased = totalReleased_.load(std::memory_order_relaxed);
        stats.peakUsage = peakUsage_.load(std::memory_order_relaxed);
        stats.utilizationPercent = (float)stats.usedObjects / (float)PoolSize * 100.0f;
        
        return stats;
    }
    
    /**
     * Reset statistics counters
     */
    void resetStatistics()
    {
        totalAcquired_.store(0, std::memory_order_relaxed);
        totalReleased_.store(0, std::memory_order_relaxed);
        peakUsage_.store(0, std::memory_order_relaxed);
    }
    
    /**
     * Check if pool has available objects
     */
    bool hasAvailableObjects() const
    {
        return nextFreeIndex_.load(std::memory_order_relaxed) >= 0;
    }
    
    /**
     * Get number of available objects
     */
    size_t getAvailableCount() const
    {
        int freeCount = nextFreeIndex_.load(std::memory_order_relaxed) + 1;
        return std::max(0, freeCount);
    }
    
private:
    // Pre-allocated object storage
    std::vector<std::unique_ptr<T>> objects_;
    
    // Free object indices stack
    std::array<int, PoolSize> freeIndices_;
    
    // Atomic index pointing to the top of the free stack
    std::atomic<int> nextFreeIndex_{-1};
    
    // Statistics
    std::atomic<uint64_t> totalAcquired_{0};
    std::atomic<uint64_t> totalReleased_{0};
    std::atomic<size_t> peakUsage_{0};
    
    void updatePeakUsage()
    {
        size_t currentUsage = PoolSize - getAvailableCount();
        size_t currentPeak = peakUsage_.load(std::memory_order_relaxed);
        
        while (currentUsage > currentPeak)
        {
            if (peakUsage_.compare_exchange_weak(currentPeak, currentUsage, std::memory_order_relaxed))
                break;
        }
    }
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ObjectPool)
};

// Forward declarations
class SpectralMask;
class PaintEngine;

/**
 * Convenience aliases for common object pools
 */
using SpectralFramePool = ObjectPool<SpectralMask::SpectralFrame, 128>;
using StrokePool = ObjectPool<PaintEngine::Stroke, 32>;
using CanvasRegionPool = ObjectPool<PaintEngine::CanvasRegion, 16>;

// Forward declarations for pool types
namespace Pools 
{
    // Global pool instances will be defined in ObjectPool.cpp
    extern SpectralFramePool* spectralFramePool;
    extern StrokePool* strokePool;
    extern CanvasRegionPool* canvasRegionPool;
    
    // Pool management functions
    void initializePools();
    void shutdownPools();
    void resetAllStatistics();
}