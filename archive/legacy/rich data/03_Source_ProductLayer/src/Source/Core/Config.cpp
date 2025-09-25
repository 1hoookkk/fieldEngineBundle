#include "Config.h" // Hook test
#include <string>
#include <cstring>

namespace SpectralCanvas {
namespace Config {
    std::atomic<LaunchMode> g_LaunchMode{LaunchMode::Normal};  // Default to normal mode for full vintage machine experience
    
    void InitializeFromCommandLine(const char* commandLine)
    {
        if (!commandLine) {
            g_LaunchMode = LaunchMode::Normal;  // Default to full experience
            return;
        }
        
        std::string cmdStr(commandLine);
        
        // Parse command line arguments
        if (cmdStr.find("--minimal") != std::string::npos) {
            g_LaunchMode = LaunchMode::Minimal;
        }
        else if (cmdStr.find("--safe") != std::string::npos) {
            g_LaunchMode = LaunchMode::Safe;
        }
        else if (cmdStr.find("--debug") != std::string::npos) {
            g_LaunchMode = LaunchMode::Debug;
        }
        else {
            g_LaunchMode = LaunchMode::Normal;
        }
    }
    
    std::string GetConfigurationSummary()
    {
        switch (g_LaunchMode.load()) {
            case LaunchMode::Minimal: return "Minimal Mode (VST3 compliance only)";
            case LaunchMode::Safe: return "Safe Mode (essential systems only)";
            case LaunchMode::Normal: return "Normal Mode (full features)";
            case LaunchMode::Debug: return "Debug Mode (all diagnostics)";
            default: return "Unknown Mode";
        }
    }
}
}