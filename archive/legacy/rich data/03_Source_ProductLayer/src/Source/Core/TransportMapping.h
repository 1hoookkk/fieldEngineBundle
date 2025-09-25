#pragma once

#include <cstdint>
#include <cmath>

namespace transport {

// Convert UI steady_clock nanos to audio samples since epoch.
inline uint64_t samplesSinceEpoch(uint64_t uiSteadyNanos,
                                  uint64_t epochSteadyNanos,
                                  double sampleRate) noexcept
{
    const double dtSec = double(int64_t(uiSteadyNanos) - int64_t(epochSteadyNanos)) / 1e9;
    if (dtSec <= 0.0) return 0;
    const double s = dtSec * sampleRate;
    return s > 0.0 ? static_cast<uint64_t>(std::llround(s)) : 0ull;
}

// Map absolute sample position to STFT column index.
inline uint32_t columnFromSamples(uint64_t samplePos, uint32_t hop) noexcept {
    return hop > 0 ? static_cast<uint32_t>(samplePos / hop) : 0u;
}

// Column within current tile (wrap).
inline uint16_t colInTile(uint32_t column, uint32_t tileWidth) noexcept
{
    return tileWidth > 0 ? static_cast<uint16_t>(column % tileWidth) : 0u;
}

} // namespace transport