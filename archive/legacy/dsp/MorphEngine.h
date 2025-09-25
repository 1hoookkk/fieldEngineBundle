#pragma once
#include <array>
#include <vector>
#include <cstdint>

namespace fe {

// Neutral, non-revealing morphing filter engine.
// Public API exposes Drive/Focus/Contour; internal surfaces are loaded from a binary blob.

struct MorphParams {
    float driveDb   = 0.0f;  // -12 .. +18
    float focus01   = 0.7f;  // 0 .. 1 (morph X / Q scale)
    float contour   = 0.0f;  // -1 .. +1 (morph Y / tilt)
    float texture01 = 0.15f; // hidden jitter; can stay constant
};

class MorphEngine {
public:
    void prepare(double sampleRate, int samplesPerBlock, int numChannels);
    void setParams(const MorphParams& p);
    void processBlock(float** channels, int numChannels, int numSamples);

    // Telemetry for UI (pulled on message thread via FIFO)
    struct Telemetry {
        float rmsL = 0.f, rmsR = 0.f;
        float peakL = 0.f, peakR = 0.f;
        float morphX = 0.0f;   // derived from focus
        float morphY = 0.5f;   // derived from contour
        bool  clipped = false;
    };
    Telemetry getAndResetTelemetry(); // thread-safe snapshot

    // Load neutral surface bank from embedded data
    void loadSurfaceBank(const void* data, size_t size);

private:
    double sr = 48000.0;
    int channels = 2;

    MorphParams params;
    Telemetry telemetry;

    // Simple biquad set per channel (vectorized if you want later)
    struct BQ { float b0=1, b1=0, b2=0, a1=0, a2=0, z1=0, z2=0; };
    std::vector<BQ> biquads; // one per channel

    // Hidden bank (neutral name)
    std::vector<uint8_t> surfaceBlob;

    // Internal helpers (intentionally generic names)
    void updateCoeffs();          // compute from params + bank
    inline float tick(BQ& s, float x) noexcept;
    inline float sat(float x) const noexcept; // drive soft clip

    // Derived mappings
    float morphX() const noexcept; // from focus01 (curved)
    float morphY() const noexcept; // from contour
    float qScale() const noexcept; // from focus01
};

} // namespace fe
