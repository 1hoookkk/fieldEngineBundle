#include "MorphEngine.h"
#include <cmath>
#include <algorithm>
#include <atomic>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace fe {

void MorphEngine::prepare(double sampleRate, int /*samplesPerBlock*/, int numChannels) {
    sr = sampleRate; channels = numChannels;
    biquads.assign((size_t)channels, {});
    updateCoeffs();
}

void MorphEngine::setParams(const MorphParams& p) {
    params = p;
    updateCoeffs();
}

inline float MorphEngine::sat(float x) const noexcept {
    // Soft clip influenced by drive (kept neutral)
    const float g = std::pow(10.0f, params.driveDb * 0.05f);
    x *= g;
    // Cubic soft clip
    const float y = x - (x*x*x) * 0.15f;
    return y * (1.0f / std::max(1.0f, g)); // simple comp
}

float MorphEngine::morphX() const noexcept { return std::pow(std::clamp(params.focus01, 0.f, 1.f), 0.85f); }
float MorphEngine::morphY() const noexcept { return 0.5f + 0.5f * std::clamp(params.contour, -1.f, 1.f); }
float MorphEngine::qScale() const noexcept { return 0.7f + 0.6f * std::clamp(params.focus01, 0.f, 1.f); }

void MorphEngine::updateCoeffs() {
    // In a real build, decode surfaceBlob and interpolate coefficient families by (morphX, morphY, qScale)
    // Here: neutral peaking-ish shape so UI can move.
    const float f0 = 1000.0f + 6000.0f * morphX(); // 1k..7k
    const float q  = 0.4f * qScale() + 0.3f;       // ~0.58..1.06
    const float g  = 0.0f;                         // flat gain (drive handled separately)

    const float w0 = 2.0f * float(M_PI) * (f0 / float(sr));
    const float alpha = std::sin(w0) / (2.0f * std::max(0.1f, q));
    const float A = std::pow(10.0f, g / 40.0f);
    const float b0 = 1 + alpha * A;
    const float b1 = -2 * std::cos(w0);
    const float b2 = 1 - alpha * A;
    const float a0 = 1 + alpha / A;
    const float a1 = -2 * std::cos(w0);
    const float a2 = 1 - alpha / A;

    for (auto& s : biquads) {
        s.b0 = b0 / a0; s.b1 = b1 / a0; s.b2 = b2 / a0;
        s.a1 = a1 / a0; s.a2 = a2 / a0;
    }
}

inline float MorphEngine::tick(BQ& s, float x) noexcept {
    const float y = s.b0*x + s.b1*s.z1 + s.b2*s.z2 - s.a1*s.z1 - s.a2*s.z2;
    s.z2 = s.z1; s.z1 = y;
    return y;
}

void MorphEngine::processBlock(float** ch, int numCh, int n) {
    float accL=0, accR=0, maxL=0, maxR=0;

    for (int i=0; i<n; ++i) {
        for (int c=0; c<numCh; ++c) {
            auto& s = biquads[(size_t)c];
            float x = ch[c][i];
            float y = tick(s, sat(x));
            ch[c][i] = y;
            if (c==0) { accL += y*y; maxL = std::max(maxL, std::abs(y)); }
            else      { accR += y*y; maxR = std::max(maxR, std::abs(y)); }
        }
    }
    telemetry.rmsL = std::sqrt(accL / std::max(1, n));
    telemetry.rmsR = std::sqrt(accR / std::max(1, n));
    telemetry.peakL = maxL; telemetry.peakR = maxR;
    telemetry.morphX = morphX(); telemetry.morphY = morphY();
    telemetry.clipped = (params.driveDb > 14.0f) && (std::max(maxL, maxR) > 0.98f);
}

MorphEngine::Telemetry MorphEngine::getAndResetTelemetry() {
    return telemetry; // simple snapshot; okay for UI polling
}

void MorphEngine::loadSurfaceBank(const void* data, size_t size) {
    surfaceBlob.assign((const uint8_t*)data, (const uint8_t*)data + size);
    updateCoeffs();
}

} // namespace fe
