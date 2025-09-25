#pragma once
#include "SpscRingBuffer.h"
#include "SampleMessage.h"
#include "MaskColumn.h"
#include "ParameterUpdate.h"
#include "SpectralFrame.h"
#include "AtlasIds.h"

// Legacy message queues (UI ↔ Audio)
using SampleQueue     = SpscRingBuffer<SampleMessage, 8>;
using MaskColumnQueue = SpscRingBuffer<MaskColumn, 128>;
using ParameterQueue  = SpscRingBuffer<ParameterUpdate, 128>;
using SpectralDataQueue = SpscRingBuffer<SpectralFrame, 16>;

// Tiled atlas message types

// Delta mask updates for specific atlas columns
struct MaskColumnDelta {
    AtlasPosition position;        // Target position in atlas
    ColumnMetadata metadata;       // Column timing and analysis data
    alignas(32) float values[AtlasConfig::NUM_BINS]; // Mask values for frequency bins
    
    MaskColumnDelta() {
        for (size_t i = 0; i < AtlasConfig::NUM_BINS; ++i) {
            values[i] = 0.0f;
        }
    }
};

// Compile-time guard: enforce column length == NUM_BINS
static_assert(sizeof(MaskColumnDelta::values) ==
              sizeof(float) * AtlasConfig::NUM_BINS,
              "MaskColumnDelta::values must be NUM_BINS floats");

// Atlas page management messages
struct AtlasPageMessage {
    enum Type : uint8_t {
        Allocate = 0,    // Request new page allocation
        Release = 1,     // Release page for reuse
        Activate = 2,    // Make page active for rendering
        Complete = 3     // Page fill complete, ready for flip
    };
    
    Type type = Allocate;
    AtlasPageHandle handle;
    uint64_t timestampUs = 0;    // When message was sent
};

// Tiled atlas message queues (Background → Audio → GPU)
using MaskDeltaQueue = SpscRingBuffer<MaskColumnDelta, 256>;    // Background → Audio
using AtlasUpdateQueue = SpscRingBuffer<AtlasUpdate, 32>;       // Audio → GPU  
using PageManagementQueue = SpscRingBuffer<AtlasPageMessage, 16>; // GPU → Background

// RT safety validation
static_assert(std::is_trivially_copyable_v<MaskColumnDelta>, "MaskColumnDelta must be trivially copyable");
static_assert(std::is_trivially_copyable_v<AtlasPageMessage>, "AtlasPageMessage must be trivially copyable");