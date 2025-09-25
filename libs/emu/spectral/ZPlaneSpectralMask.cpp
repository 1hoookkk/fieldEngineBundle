#include "ZPlaneSpectralMask.h"

#include <cassert>
#include <cmath>

namespace zplane {

static constexpr float kRmax = 0.995f; // stability clamp

void ZPlaneSpectralMask::prepare(double sr, int fftSize, int numBins,
                                 const ZPlaneCoefficientBank* bank) {
    bank_ = bank; sampleRate_ = sr; fftSize_ = fftSize; numBins_ = numBins;
    tmplA_.assign(256, 1.0f);
    tmplB_.assign(256, 1.0f);
    maskMag_.assign((size_t)numBins_, 1.0f);
    lut_.build((float)sr, fftSize, numBins, 20.f, (float)(0.5 * sr), 256);
    dirtyModels_ = true;
    morphSmoothed_ = morphTarget_.load(std::memory_order_relaxed);
}

void ZPlaneSpectralMask::process(std::complex<float>* spec) {
    DenormalFTZScope ftz; // avoid denormal spikes

    // Smooth morph (per-hop time using hop size if provided)
    const float target = morphTarget_.load(std::memory_order_relaxed);
    const float hopSamples = (float)(hopSamples_ > 0 ? hopSamples_ : fftSize_);
    const float hopSec = hopSamples / (float)sampleRate_;
    const float alpha  = 1.0f - std::exp(-hopSec / (0.001f * std::max(1.0f, morphSmoothTauMs_)));
    morphSmoothed_ += alpha * (target - morphSmoothed_);

    // Rebuild templates if endpoints changed
    rebuildTemplatesIfNeeded();

    // Morph templates in log-f domain; interpolate either in linear or dB domain
    const float t = morphSmoothed_;
    for (int k = 0; k < numBins_; ++k) {
        float idx = lut_.binToIdx[(size_t)k];
        int i1 = (int)std::floor(idx);
        float frac = idx - i1;
        int i0 = std::max(0, i1 - 1);
        int i2 = std::min(i1 + 1, 255);
        int i3 = std::min(i1 + 2, 255);

        if (quality_ == SpectralMaskQuality::Template256dB) {
            auto toDb   = [](float x) noexcept { return 20.0f * std::log10(std::max(x, 1.0e-12f)); };
            auto fromDb = [](float db) noexcept { return std::pow(10.0f, db / 20.0f); };
            float d0 = (1.f - t) * toDb(tmplA_[(size_t)i0]) + t * toDb(tmplB_[(size_t)i0]);
            float d1 = (1.f - t) * toDb(tmplA_[(size_t)i1]) + t * toDb(tmplB_[(size_t)i1]);
            float d2 = (1.f - t) * toDb(tmplA_[(size_t)i2]) + t * toDb(tmplB_[(size_t)i2]);
            float d3 = (1.f - t) * toDb(tmplA_[(size_t)i3]) + t * toDb(tmplB_[(size_t)i3]);
            float magDb = cubicHermite(d0, d1, d2, d3, frac);
            float mag   = fromDb(magDb);
            maskMag_[(size_t)k] = std::max(mag, 0.0f);
        } else {
            float a0 = (1.f - t) * tmplA_[(size_t)i0] + t * tmplB_[(size_t)i0];
            float a1 = (1.f - t) * tmplA_[(size_t)i1] + t * tmplB_[(size_t)i1];
            float a2 = (1.f - t) * tmplA_[(size_t)i2] + t * tmplB_[(size_t)i2];
            float a3 = (1.f - t) * tmplA_[(size_t)i3] + t * tmplB_[(size_t)i3];
            float mag = cubicHermite(a0, a1, a2, a3, frac);
            maskMag_[(size_t)k] = std::max(mag, 0.0f);
        }
    }

    // Apply spectral mask (multiply magnitude only; keep phase)
    for (int k = 0; k < numBins_; ++k) {
        spec[(size_t)k] *= maskMag_[(size_t)k];
    }
}

void ZPlaneSpectralMask::rebuildTemplatesIfNeeded() {
    if (!dirtyModels_) return;
    const std::uint16_t a = modelA_.load(std::memory_order_relaxed);
    const std::uint16_t b = modelB_.load(std::memory_order_relaxed);
    buildTemplateForModel(a, tmplA_);
    buildTemplateForModel(b, tmplB_);
    dirtyModels_ = false;
}

// Evaluate |H(e^{jw})| on 256 log-f points for one model ID (conjugate-pair exact magnitude)
void ZPlaneSpectralMask::buildTemplateForModel(std::uint16_t modelId, std::vector<float>& outMag256) {
    assert(bank_ && modelId < bank_->modelCount());
    const auto& M = bank_->getModel(modelId);

    outMag256.resize(256);
    for (int i = 0; i < 256; ++i) {
        const float f = lut_.gridHz[(size_t)i];
        const float w = 2.f * 3.14159265358979323846f * (f / (float)sampleRate_);

        float mag = M.overallGain;

        for (int s = 0; s < (int)M.numSections; ++s) {
            const float rp = std::min(M.s[(size_t)s].poleRadius, kRmax);
            const float rz = std::clamp(M.s[(size_t)s].zeroRadius, 0.0f, 1.0f);
            const float ap = M.s[(size_t)s].poleAngle;
            const float az = M.s[(size_t)s].zeroAngle;

            auto conjPairDist = [](float r, float a, float wRad) noexcept {
                const float t1 = 1.0f + r * r - 2.0f * r * std::cos(wRad - a);
                const float t2 = 1.0f + r * r - 2.0f * r * std::cos(wRad + a);
                return std::sqrt(std::max(t1, 0.0f)) * std::sqrt(std::max(t2, 0.0f));
            };

            const float num = (rz > 0.0f) ? conjPairDist(rz, az, w) : 1.0f;
            const float den = conjPairDist(rp, ap, w);
            const float Hs  = (den > 1.0e-20f) ? (num / den) : 1.0f;
            mag *= (M.s[(size_t)s].sectionGain * Hs);
        }
        outMag256[(size_t)i] = mag;
    }
}

void ZPlaneSpectralMask::interpolatePolar(const Model& A, const Model& B, float t, Model& out) {
    const int N = std::min((int)A.numSections, (int)B.numSections);
    out.numSections = (std::uint8_t)N;
    for (int i = 0; i < N; ++i) {
        out.s[(size_t)i].poleRadius = std::min(A.s[(size_t)i].poleRadius + t * (B.s[(size_t)i].poleRadius - A.s[(size_t)i].poleRadius), kRmax);
        out.s[(size_t)i].poleAngle  = interpAngle(A.s[(size_t)i].poleAngle,  B.s[(size_t)i].poleAngle,  t);
        out.s[(size_t)i].zeroRadius = std::clamp(A.s[(size_t)i].zeroRadius + t * (B.s[(size_t)i].zeroRadius - A.s[(size_t)i].zeroRadius), 0.0f, 1.0f);
        out.s[(size_t)i].zeroAngle  = interpAngle(A.s[(size_t)i].zeroAngle,  B.s[(size_t)i].zeroAngle,  t);
        out.s[(size_t)i].sectionGain= A.s[(size_t)i].sectionGain + t * (B.s[(size_t)i].sectionGain - A.s[(size_t)i].sectionGain);
    }
    out.overallGain = A.overallGain + t * (B.overallGain - A.overallGain);
}

} // namespace zplane


