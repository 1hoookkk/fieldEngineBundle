#include "RealtimeMemoryManager.h"

// Implementation for RealtimeMemoryManager
// Most functionality is header-only for performance

// Initialize global memory system
RealtimeMemorySystem& getGlobalMemorySystem()
{
    return RealtimeMemorySystem::getInstance();
}

// Utility functions for memory debugging
void printMemoryReport()
{
    RealtimeMemorySystem::getInstance().printMemoryReport();
}

// Memory verification for debug builds
#ifdef JUCE_DEBUG
static std::atomic<bool> g_realtimeContextActive{false};

void setRealtimeContext(bool active)
{
    g_realtimeContextActive.store(active, std::memory_order_relaxed);
}

bool isInRealtimeContext()
{
    return g_realtimeContextActive.load(std::memory_order_relaxed);
}

// Override global new/delete in debug builds to catch allocations
void* operator new(size_t size)
{
    if (isInRealtimeContext())
    {
        // This will help catch accidental allocations in real-time context
        jassertfalse; // In debug builds, this will halt execution
        DBG("ERROR: Memory allocation in real-time context! Size: " << size);
    }
    
    return std::malloc(size);
}

void operator delete(void* ptr) noexcept
{
    if (isInRealtimeContext())
    {
        DBG("WARNING: Memory deallocation in real-time context!");
    }
    
    std::free(ptr);
}

#else
// Release builds - no overhead
void setRealtimeContext(bool) {}
bool isInRealtimeContext() { return false; }
#endif