#pragma once

// CLAUDE.md Core Practice 1: CrashToggles First
// All stability switches live here. Default = safest setting until explicitly cleared.

namespace CrashToggles {
    
    // Paint-to-Audio Core Safety (minimal enabled for HUD validation)
    constexpr bool ENABLE_PAINT_QUEUE = true;          // RT-safe paint event queue - ENABLED for HUD testing
    constexpr bool ENABLE_SPECTRAL_SYNTHESIS = false;  // Core paint→audio engine - DISABLED for baseline
    constexpr bool ENABLE_SPECTRAL_ENGINE = true;      // SpectralSynthEngine constructor safety - ENABLED for HUD testing
    constexpr bool ENABLE_MASKING_ENGINE = false;      // Sample masking system - DISABLED for baseline
    
    // UI Safety Switches (minimal enabled for HUD validation)
    constexpr bool ENABLE_TEXTURE_LOADING = false;     // Asset/texture system - DISABLED for stability
    constexpr bool ENABLE_COMPLEX_UI = false;          // Rooms, headers, overlays - DISABLED
    constexpr bool ENABLE_CANVAS_COMPONENT = true;     // CanvasComponent - ENABLED for HUD testing
    constexpr bool ENABLE_CANVAS_RENDERING = false;    // Canvas rendering system - DISABLED
    constexpr bool ENABLE_EDITOR_TIMERS = true;        // Timers - ENABLED for HUD validation
    
    // Audio Thread Safety (minimal for baseline)
    constexpr bool ENABLE_RT_ASSERTIONS = false;       // Real-time safety checks - DISABLED for isolation
    constexpr bool ENABLE_AUDIO_LOGGING = false;       // Never log on audio thread
    constexpr bool ENABLE_HEAP_TRACKING = false;       // Track allocations - DISABLED for isolation
    
    // Development & Debug (minimal for baseline)
    constexpr bool ENABLE_SNAPSHOT_HARNESS = false;    // Auto-capture system - DISABLED for isolation
    constexpr bool ENABLE_HUD_COUNTERS = true;         // Performance counters - ENABLED for validation
    constexpr bool ENABLE_DEVELOPER_HUD = true;        // Developer HUD with RT telemetry - ENABLED for diagnostics
    constexpr bool ENABLE_PERFORMANCE_COUNTERS = false; // Performance monitoring - DISABLED for isolation
    constexpr bool ENABLE_MULTICORE_DSP = false;       // Multicore DSP processing - DISABLED for baseline stability
    constexpr bool ENABLE_PLUGINVAL_STRICT = true;     // Strict validation
    
    // Feature Gates (all disabled for baseline stability)
    constexpr bool ENABLE_REALMDIAL = false;           // RealmDial component - DISABLED
    constexpr bool ENABLE_REALM_SWAP = false;          // Realm switching - DISABLED
    constexpr bool ENABLE_UNDO_SYSTEM = false;         // Command undo/redo - DISABLED for isolation
    constexpr bool ENABLE_TRACKER_SEQUENCING = false;  // Tracker sequencing engine - DISABLED for isolation
    constexpr bool ENABLE_EMU_ROMPLER = true;          // EMU rompler engine - ENABLED for Z-plane implementation
}