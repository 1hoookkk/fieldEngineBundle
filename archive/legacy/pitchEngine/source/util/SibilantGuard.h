#pragma once
#include <cmath>
#include <algorithm>

// Cheap HF ratio guard prevents "spray can" sound on sibilants:
// Use first-difference as 6dB/oct high-pass proxy. Compute |d| energy vs |x| energy.
// Map ratio -> weight in [0.7..1.0] so sibilants get *slightly* less correction.
struct SibilantGuard
{
    float weight(const float* x, int n)
    {
        if (!x || n <= 1) return 1.0f;

        float e  = 0.0f;  // broadband abs energy
        float eh = 0.0f;  // "HF" abs energy via first difference
        float prev = x[0];

        for (int i = 0; i < n; ++i)
        {
            const float xi = x[i];
            const float d  = xi - prev;
            prev = xi;
            e  += std::abs(xi);
            eh += std::abs(d);
        }

        if (e <= 1e-6f) return 1.0f;

        float r = std::clamp(eh / e, 0.0f, 1.0f);          // 0..1
        // Soft knee: start easing above ~0.25, reach 0.7 weight near ~0.7
        float t = std::clamp((r - 0.25f) / (0.70f - 0.25f), 0.0f, 1.0f);
        return 1.0f - 0.30f * t;                            // 1.0 .. 0.7
    }
};