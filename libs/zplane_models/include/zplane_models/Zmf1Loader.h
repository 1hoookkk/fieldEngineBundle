#pragma once
#include <cstdint>
#include <array>
#include <cmath>
#include <cstring>

namespace pe {

struct Biquad5 { float b0, b1, b2, a1, a2; };
static constexpr int kMaxSections = 6;
static constexpr int kMaxFrames   = 64; // plenty; we'll likely use 11

class Zmf1Loader {
public:
    bool loadFromMemory(const uint8_t* data, size_t size); // call off audio thread
    // outCoeffs must have kMaxSections entries; morph in [0,1]
    void getCoefficients(float morph, float sr, std::array<Biquad5,kMaxSections>& out) const noexcept;

    int  numFrames()   const noexcept { return numFrames_; }
    int  numSections() const noexcept { return numSections_; }
    int  modelId()     const noexcept { return modelId_; }
    float refSR()      const noexcept { return sampleRateRef_; }

private:
    // Stored exactly as read; no allocations after load
    int   modelId_ = -1;
    int   numFrames_ = 0;
    int   numSections_ = 0;
    float sampleRateRef_ = 48000.0f;

    // frames[frame][section]
    std::array<std::array<Biquad5, kMaxSections>, kMaxFrames> frames_{};

    // Simple, fast fractional-lag interpolation
    static inline float lerp(float a, float b, float t) noexcept { return a + t*(b - a); }
};

} // namespace pe