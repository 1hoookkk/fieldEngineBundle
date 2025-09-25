#pragma once

#include <cstdlib>
#include <atomic>
#include <string>

namespace SpectralCanvas {
namespace Config {

    // Launch mode detection (minimal for baseline)
    enum class LaunchMode {
        Minimal = 0,  // VST3 compliance only
        Safe = 1,     // Essential systems
        Normal = 2,   // Full features  
        Debug = 3     // All diagnostics
    };
    
    extern std::atomic<LaunchMode> g_LaunchMode;
    
    // Basic mode queries
    inline bool IsMinimalModeActive() { return g_LaunchMode.load() == LaunchMode::Minimal; }
    inline bool IsSafeModeActive() { return g_LaunchMode.load() == LaunchMode::Safe; }
    inline bool IsFullModeActive() { return g_LaunchMode.load() == LaunchMode::Normal; }
    inline bool IsDebugModeActive() { return g_LaunchMode.load() == LaunchMode::Debug; }
    
    // Environment variable overrides (minimal)
    static bool IsMulticoreDspEnabled() {
        if (g_LaunchMode.load() == LaunchMode::Minimal || g_LaunchMode.load() == LaunchMode::Safe) {
            return false;  // Hard-off for baseline stability
        }
        const char* forceSingle = std::getenv("SC_FORCE_SINGLECORE");
        if (forceSingle && std::atoi(forceSingle) != 0) return false;
        const char* forceMulti = std::getenv("SC_FORCE_MULTICORE");
        if (forceMulti && std::atoi(forceMulti) != 0) return true;
        return false;  // Default OFF for baseline stability
    }
    
    // Feature namespaces (minimal stubs)
    namespace EngineFeatures {
        inline bool IsSpectralEngineEnabled() { 
            return IsFullModeActive() || IsDebugModeActive(); 
        }
        inline bool IsMulticoreDspEnabled() { return Config::IsMulticoreDspEnabled(); }
        inline bool IsEffectsRackEnabled() { 
            return IsFullModeActive() || IsDebugModeActive(); 
        }
        inline bool IsWavetableSynthEnabled() { 
            return IsFullModeActive() || IsDebugModeActive(); 
        }
    }
    
    namespace UIFeatures {
        inline bool IsCanvasRenderingEnabled() { 
            return IsFullModeActive() || IsDebugModeActive(); 
        }
        inline bool IsAdvancedGraphicsEnabled() { 
            return IsFullModeActive() || IsDebugModeActive(); 
        }
    }
    
    // Hierarchical safety checks (enable for full experience)
    inline bool ShouldAllocateSpectralEngines() { 
        return IsFullModeActive() || IsDebugModeActive(); 
    }
    inline bool ShouldInitializeLayerManager() { 
        return IsFullModeActive() || IsDebugModeActive(); 
    }
    inline bool ShouldCreateComplexUI() { 
        return IsFullModeActive() || IsDebugModeActive(); 
    }
    
    // Command line initialization
    void InitializeFromCommandLine(const char* commandLine);
    std::string GetConfigurationSummary();

} // namespace Config
} // namespace SpectralCanvas