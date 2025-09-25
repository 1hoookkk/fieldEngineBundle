#include "AtomicOscillator.h"

// Implementation file for AtomicOscillator
// Most functionality is header-only for performance (inlining)

// 🚨 EMERGENCY FIX: Replace global static with function-local static to avoid SIOF
// Global oscillator bank pool instance for the application
// OscillatorBankPool<256> g_oscillatorBankPool;  // REMOVED - causes static initialization order fiasco

// Function to get the oscillator bank pool instance (thread-safe initialization)
OscillatorBankPool<256>& getOscillatorBankPool() noexcept
{
    static OscillatorBankPool<256> instance;
    return instance;
}

// Statistics tracking for performance monitoring
static std::atomic<uint64_t> g_totalSamplesProcessed{0};
static std::atomic<uint64_t> g_totalParameterUpdates{0};

void incrementSampleCount(uint64_t samples) noexcept
{
    // 🚨 EMERGENCY FIX: Use seq_cst instead of relaxed for safety
    g_totalSamplesProcessed.fetch_add(samples, std::memory_order_seq_cst);
}

void incrementParameterUpdateCount() noexcept
{
    // 🚨 EMERGENCY FIX: Use seq_cst instead of relaxed for safety
    g_totalParameterUpdates.fetch_add(1, std::memory_order_seq_cst);
}

uint64_t getTotalSamplesProcessed() noexcept
{
    // 🚨 EMERGENCY FIX: Use seq_cst instead of relaxed for safety
    return g_totalSamplesProcessed.load(std::memory_order_seq_cst);
}

uint64_t getTotalParameterUpdates() noexcept
{
    // 🚨 EMERGENCY FIX: Use seq_cst instead of relaxed for safety
    return g_totalParameterUpdates.load(std::memory_order_seq_cst);
}