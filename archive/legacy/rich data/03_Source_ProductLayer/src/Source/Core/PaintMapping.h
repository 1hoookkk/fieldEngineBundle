#pragma once

#include <algorithm>
#include <cmath>

namespace paint {

// Log-frequency Y→bin with top=high (invert y in mapping).
// f in [80, 8000] Hz mapped into [0 .. fftSize/2].
inline int uiYToBinTopHigh(float yNorm, double sampleRate, int fftSize) noexcept
{
    constexpr float fMin = 80.0f, fMax = 8000.0f;
    const float nyq = static_cast<float>(sampleRate * 0.5);
    const float y   = std::clamp(1.0f - yNorm, 0.0f, 1.0f); // top=high
    const float f   = fMin * std::pow(fMax / fMin, y);
    const int   bin = static_cast<int>(std::lround((f / nyq) * float(fftSize / 2)));
    return std::clamp(bin, 0, fftSize / 2);
}

// 3-tap triangular "splat" around center bin (audible, RT-safe).
inline void splatTri3(float* values, int numBins, int centerBin, float intensity) noexcept
{
    if (!values || numBins <= 2) return;
    const int k = std::clamp(centerBin, 1, numBins - 2);
    const float c = std::clamp(intensity, 0.0f, 1.0f);
    values[k - 1] = std::max(values[k - 1], 0.25f * c);
    values[k]     = std::max(values[k],     0.60f * c);
    values[k + 1] = std::max(values[k + 1], 0.25f * c);
}

} // namespace paint