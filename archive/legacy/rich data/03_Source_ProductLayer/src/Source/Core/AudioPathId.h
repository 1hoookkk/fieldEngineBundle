#pragma once

enum class AudioPathId : uint8_t 
{
    Unknown = 0,
    TestFeeder = 1, 
    Phase4Synth = 2,
    SpectralResynthesis = 3,
    SpectralEngineStub = 4,
    ModernPaint = 5,
    Hybrid = 6,
    Silent = 7
};

inline const char* audioPathName(AudioPathId pathId) noexcept
{
    switch (pathId) {
        case AudioPathId::TestFeeder: return "TestFeeder";
        case AudioPathId::Phase4Synth: return "Phase4Synth";
        case AudioPathId::SpectralResynthesis: return "SpectralResynthesis";
        case AudioPathId::SpectralEngineStub: return "SpectralEngineStub";
        case AudioPathId::ModernPaint: return "ModernPaint";
        case AudioPathId::Hybrid: return "Hybrid";
        case AudioPathId::Silent: return "Silent";
        default: return "Unknown";
    }
}