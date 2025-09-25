#pragma once

#include <cmath>
#if defined(__SSE__) || defined(__SSE2__)
  #include <xmmintrin.h>
#endif
#include <algorithm>

namespace zplane {

inline float wrapAngle(float a) {
    const float pi = 3.14159265358979323846f;
    a = std::fmod(a + pi, 2.f * pi);
    if (a < 0) a += 2.f * pi;
    return a - pi;
}

inline float interpAngle(float a0, float a1, float t) {
    float d = wrapAngle(a1 - a0);
    return a0 + t * d;
}

inline float cubicHermite(float y0, float y1, float y2, float y3, float t) {
    float a0 = -0.5f * y0 + 1.5f * y1 - 1.5f * y2 + 0.5f * y3;
    float a1 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    float a2 = -0.5f * y0 + 0.5f * y2;
    float a3 = y1;
    return ((a0 * t + a1) * t + a2) * t + a3;
}

struct DenormalFTZScope {
#if defined(__SSE__) || defined(__SSE2__)
    unsigned int oldMXCSR { 0 };
    DenormalFTZScope() {
        oldMXCSR = _mm_getcsr();
        _mm_setcsr(oldMXCSR | 0x8040); // FTZ | DAZ
    }
    ~DenormalFTZScope() { _mm_setcsr(oldMXCSR); }
#else
    DenormalFTZScope() {}
    ~DenormalFTZScope() {}
#endif
};

} // namespace zplane


