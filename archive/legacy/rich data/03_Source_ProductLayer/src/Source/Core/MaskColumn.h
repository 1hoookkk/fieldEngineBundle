#pragma once
#include <cstdint>
#include <type_traits>
#include "AtlasIds.h"

/**
 * POD used for UI→Audio mask updates; keep trivially copyable.
 * Compatible with lock-free queues for RT-safe communication.
 */
struct MaskColumn {
    static constexpr size_t MAX_BINS = AtlasConfig::NUM_BINS; // Match atlas (FFT_SIZE/2 + 1)
    
    alignas(32) float values[MAX_BINS]; // Spectral mask values
    
    int   binStart   = 0;     // inclusive bin index
    int   binEnd     = 0;     // exclusive bin index  
    float intensity  = 0.0f;  // 0..1 mask intensity
    int   x          = 0;     // optional: UI column index
    uint32_t frameIndex = 0;  // frame/sequence number
    uint32_t sequenceNumber = 0; // ordering sequence
    uint32_t numBins = 0;     // number of valid bins
    uint64_t timestampSamples = 0; // timestamp in samples
    uint64_t uiTimestampMicros = 0; // UI timestamp in microseconds
    
    MaskColumn() {
        for (size_t i = 0; i < MAX_BINS; ++i) {
            values[i] = 0.0f;
        }
    }
};

// Ensure RT safety
static_assert(std::is_trivially_copyable_v<MaskColumn>, "MaskColumn must be trivially copyable for RT safety");
static_assert(std::is_standard_layout_v<MaskColumn>, "MaskColumn must have standard layout");