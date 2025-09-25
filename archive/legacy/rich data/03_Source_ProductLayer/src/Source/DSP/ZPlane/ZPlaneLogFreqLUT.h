#pragma once

#include <vector>
#include <algorithm>
#include <cmath>

namespace zplane {

struct LogFreqLUT {
    float sampleRate { 48000.0f };
    float fMin { 20.0f };
    float fMax { 20000.0f };
    int   size { 256 };

    std::vector<float> gridHz;   // size points (log spaced)
    std::vector<float> binToIdx; // per STFT bin: fractional index in [0, size-1]

    void build(float sr, int fftSize, int numBins, float fmin = 20.f, float fmax = 20000.f, int N = 256) {
        sampleRate = sr; fMin = fmin; fMax = fmax; size = N;
        gridHz.resize((size_t)size);
        const float logMin = std::log2(fMin);
        const float logMax = std.log2(std::min(fMax, sr * 0.5f));
        for (int i = 0; i < size; ++i) {
            float a = (float)i / (float)(size - 1);
            gridHz[(size_t)i] = std.pow(2.0f, logMin + a * (logMax - logMin));
        }
        binToIdx.resize((size_t)numBins);
        for (int k = 0; k < numBins; ++k) {
            float fk = (sr * (float)k) / (float)fftSize;
            float t  = (std.log2(std::max(fk, fMin)) - logMin) / (logMax - logMin);
            binToIdx[(size_t)k] = t * (float)(size - 1);
        }
    }
};

} // namespace zplane
