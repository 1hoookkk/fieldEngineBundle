#pragma once

#include <vector>
#include <atomic>
#include <complex>
#include <algorithm>
#include <cstdint>

#include "ZPlaneCoefficientBank.h"
#include "ZPlaneLogFreqLUT.h"

namespace zplane {

// Quality mode: template upsample (fast), optional dB-domain morph (more perceptual)
enum class SpectralMaskQuality : std::uint8_t { Template256 = 0, Template256dB = 1 /*, DirectSOS*/ };

class ZPlaneSpectralMask {
public:
    void prepare(double sampleRate, int fftSize, int numBins, const ZPlaneCoefficientBank* bank);

    // Control (atomics -> smoothed per hop)
    void setModels(std::uint16_t modelA, std::uint16_t modelB) noexcept {
        modelA_.store(modelA, std::memory_order_relaxed);
        modelB_.store(modelB, std::memory_order_relaxed);
        dirtyModels_ = true;
    }
    void setMorph(float m) noexcept { morphTarget_.store(std::clamp(m, 0.0f, 1.0f)); }
    void setQuality(SpectralMaskQuality q) noexcept { quality_ = q; }
    void setHopSize(int hopSamples) noexcept { hopSamples_ = hopSamples; }

    // Per hop (audio thread). Multiplies spectrum in-place with |H| mask.
    void process(std::complex<float>* spec /*length = numBins_*/);

private:
    void rebuildTemplatesIfNeeded();
    void buildTemplateForModel(std::uint16_t modelId, std::vector<float>& outMag256);
    void interpolatePolar(const Model& A, const Model& B, float t, Model& out);

    // Config
    const ZPlaneCoefficientBank* bank_ { nullptr };
    double sampleRate_ { 48000.0 };
    int fftSize_ { 2048 };
    int numBins_ { 1025 };

    // Control state
    std::atomic<std::uint16_t> modelA_ { 0 }, modelB_ { 1 };
    std::atomic<float>         morphTarget_ { 0.0f };
    float                      morphSmoothed_ { 0.0f };
    float                      morphSmoothTauMs_ { 8.0f }; // 2–20ms control smoothing
    SpectralMaskQuality        quality_ { SpectralMaskQuality::Template256 };
    int                        hopSamples_ { 0 }; // samples per hop (H)

    // Precomputed frequency mapping
    LogFreqLUT lut_;

    // Templates for each endpoint
    std::vector<float> tmplA_; // 256
    std::vector<float> tmplB_; // 256
    bool dirtyModels_ { true };

    // Working buffer (bin magnitudes)
    std::vector<float> maskMag_; // size = numBins_
};

} // namespace zplane


