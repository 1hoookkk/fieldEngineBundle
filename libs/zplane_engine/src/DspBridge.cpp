#include "zplane_engine/DspBridge.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstring>

#include <BinaryData.h>
#include <sysex/vendors/ProteusLayerFilter.h>

#include "zplane/ZPoleMath.h"

using namespace pe;

namespace
{
    using pe::Biquad5;

    constexpr double kTwoPi = 6.28318530717958647692;
    constexpr double kPassivityTarget = 0.98;
    constexpr double kPassivityEps = 1.0e-9;
    constexpr double kPassivityFloor = 0.35;

    struct EmbeddedPack
    {
        int index;
        const void* data;
        size_t size;
    };

    static const EmbeddedPack kEmbeddedPacks[] = {
        { 0, BinaryData::vowel_pair_zmf1, BinaryData::vowel_pair_zmf1Size },
        { 1, BinaryData::bell_pair_zmf1,  BinaryData::bell_pair_zmf1Size },
        { 2, BinaryData::low_pair_zmf1,   BinaryData::low_pair_zmf1Size },
    };

    bool loadEmbeddedPackForIndex(int idx, Zmf1Loader& loader)
    {
        for (const auto& pack : kEmbeddedPacks)
        {
            if (pack.index == idx && pack.data != nullptr && pack.size > 0)
                return loader.loadFromMemory(reinterpret_cast<const uint8_t*>(pack.data), pack.size);
        }
        return false;
    }

    inline std::complex<float> firstPole(const Biquad5& section) noexcept
    {
        const float a1 = section.a1;
        const float a2 = section.a2;
        std::complex<float> discriminant { a1 * a1 - 4.0f * a2, 0.0f };
        discriminant = std::sqrt(discriminant);
        return (-a1 + discriminant) * 0.5f;
    }

    inline Biquad5 remapSectionForSampleRate(const Biquad5& section, float targetSR, float referenceSR) noexcept
    {
        if (targetSR <= 0.0f || std::abs(targetSR - referenceSR) < 1.0f)
            return section;

        if (std::abs(referenceSR - zpm::kRefFs) > 1.0f)
            return section; // only supporting 48k reference tables for now

        std::complex<float> pole = firstPole(section);
        if (!std::isfinite(pole.real()) || !std::isfinite(pole.imag()))
            return section;

        const auto mapped = zpm::remapPolar48kToFs(std::abs(pole), std::arg(pole), targetSR);
        const std::complex<float> mappedPole = std::polar(mapped.first, mapped.second);

        Biquad5 result = section;
        result.a1 = -2.0f * std::real(mappedPole);
        result.a2 = std::clamp(static_cast<float>(std::norm(mappedPole)), -0.9999f, 0.9999f);
        return result;
    }
}
    inline void applyResonanceToSection(Biquad5& section, float resonance) noexcept
    {
        if (!std::isfinite(section.a1) || !std::isfinite(section.a2))
            return;
        if (section.a2 <= 0.0f)
            return;

        const float discriminant = section.a1 * section.a1 - 4.0f * section.a2;
        if (discriminant >= 0.0f)
            return;

        const float rMin = 0.2f;
        const float rMax = 0.9995f;
        float r = std::sqrt(section.a2);
        r = std::clamp(r, rMin, rMax);
        if (r < 1.0e-4f)
            return;

        const float denom = -2.0f * r;
        float cosTheta = 1.0f;
        if (std::abs(denom) > 1.0e-6f)
            cosTheta = std::clamp(section.a1 / denom, -1.0f, 1.0f);

        const float re = std::clamp(resonance, 0.0f, 1.0f);
        const float k = 0.8f;
        const float expK = std::clamp(1.0f - k * (re - 0.5f), 0.25f, 1.75f);

        float rPrime = std::pow(r, expK);
        rPrime = std::clamp(rPrime, rMin, rMax);

        section.a1 = -2.0f * rPrime * cosTheta;
        section.a2 = rPrime * rPrime;
    }

    inline void applyResonance(pe::Biquad5* sections, int numSections, float resonance) noexcept
    {
        if (sections == nullptr || numSections <= 0)
            return;
        for (int i = 0; i < numSections; ++i)
            applyResonanceToSection(sections[i], resonance);
    }


void DspBridge::reset() noexcept
{
    cascadeL_.reset();
    cascadeR_.reset();
    std::fill(dryBuffer_.begin(), dryBuffer_.end(), 0.0f);
    passivityGain_ = 1.0f;
}

bool DspBridge::loadModelByBinarySymbol(int modelIndex)
{
    if (loadEmbeddedPackForIndex(modelIndex, loader_))
    {
        reset();
        return true;
    }
    return false;
}

bool DspBridge::apply(const emu_sysex::LayerFilter14& filter, const fe::morphengine::EmuMapConfig& cfg) noexcept
{
    (void) cfg;
    const int packCount = static_cast<int>(std::size(kEmbeddedPacks));
    auto tryLoad = [&](int rawIndex) -> bool
    {
        if (packCount <= 0)
            return false;
        if (rawIndex < 0)
            return false;
        const int wrapped = rawIndex % packCount;
        if (loadModelByBinarySymbol(wrapped))
        {
            lastModelIndex_ = wrapped;
            return true;
        }
        return false;
    };

    if (tryLoad(static_cast<int>(filter.morphIndex)))
        return true;
    if (tryLoad(static_cast<int>(filter.filterType)))
        return true;

    if (lastModelIndex_ >= 0)
        return true;

    return tryLoad(0);
}

float DspBridge::mapCutoffToFreq(float norm01) noexcept
{
    const float minHz = 25.0f;
    const float maxHz = 16000.0f;
    const float ratio = maxHz / minHz;
    const float clamped = std::clamp(norm01, 0.0f, 1.0f);
    return minHz * std::pow(ratio, clamped);
}

float DspBridge::mapResonanceToQ(float norm01) noexcept
{
    const float clamped = std::clamp(norm01, 0.0f, 1.0f);
    return 0.5f + clamped * 11.5f;
}

void DspBridge::process(float* const* io, int numCh, int numSamp, const ZPlaneParams& p) noexcept
{
    if (io == nullptr || numCh <= 0 || numSamp <= 0)
        return;

    if (p.mix <= 1.0e-4f && std::abs(p.resonance) <= 1.0e-4f)
        return; // effectively transparent

    const int channelCount = std::max(1, numCh);
    const int capacityPerChannel = static_cast<int>(dryBuffer_.size()) / channelCount;
    if (capacityPerChannel <= 0)
        return;

    if (numSamp > capacityPerChannel)
    {
        int offset = 0;
        while (offset < numSamp)
        {
            const int chunk = std::min(capacityPerChannel, numSamp - offset);
            float* chunkPtrs[2] = {
                io[0] + offset,
                numCh > 1 ? io[1] + offset : nullptr
            };
            process(chunkPtrs, numCh, chunk, p);
            offset += chunk;
        }
        return;
    }

    const bool needsDry = (p.mix < 0.999f);
    if (needsDry)
    {
        for (int ch = 0; ch < numCh; ++ch)
            std::memcpy(dryBuffer_.data() + ch * numSamp, io[ch], static_cast<size_t>(numSamp) * sizeof(float));
    }

    if (loader_.numSections() <= 0)
    {
        if (needsDry)
            mixInPlace(io, numCh, numSamp, dryBuffer_.data(), p.mix);
        return;
    }

    std::array<Biquad5, kMaxSections> sections {};
    applyResonance(sections.data(), loader_.numSections(), p.resonance);
    loader_.getCoefficients(p.morph, sr_, sections);

    const float refSR = loader_.refSR();
    const bool needsRemap = std::abs(sr_ - refSR) > 1.0f;

    for (int i = 0; i < loader_.numSections(); ++i)
    {
        Biquad5 section = sections[i];
        if (needsRemap)
            section = remapSectionForSampleRate(section, sr_, refSR);

        cascadeL_.s[i].b0 = section.b0;
        cascadeL_.s[i].b1 = section.b1;
        cascadeL_.s[i].b2 = section.b2;
        cascadeL_.s[i].a1 = section.a1;
        cascadeL_.s[i].a2 = section.a2;

        const bool stable = (std::abs(section.a2) < 0.9999f) && (std::abs(section.a1) < 1.9999f);
        if (!stable)
        {
            std::printf("Warning: Section %d unstable (a1=%f, a2=%f)\n", i, section.a1, section.a2);
            cascadeL_.s[i].b0 = 1.0f;
            cascadeL_.s[i].b1 = 0.0f;
            cascadeL_.s[i].b2 = 0.0f;
            cascadeL_.s[i].a1 = 0.0f;
            cascadeL_.s[i].a2 = 0.0f;
            sections[i] = { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f };
        }
        else
        {
            sections[i] = section;
        }

        if (numCh > 1)
            cascadeR_.s[i] = cascadeL_.s[i];
    }

    const float targetGain = estimatePassivityScalar(sections.data(), loader_.numSections(), sr_);
    const float startGain = passivityGain_;

    float endGain = targetGain;
    if (sr_ > 0.0f)
    {
        const double tauSeconds = 0.02; // 20 ms ramp
        const double alpha = std::exp(-(static_cast<double>(numSamp) / (tauSeconds * sr_)));
        endGain = static_cast<float>(targetGain + (startGain - targetGain) * alpha);
    }
    const float step = (numSamp > 0) ? (endGain - startGain) / static_cast<float>(numSamp) : 0.0f;

    for (int n = 0; n < numSamp; ++n)
    {
        const float g = std::clamp(startGain + step * static_cast<float>(n), 0.0f, 1.0f);
        io[0][n] = cascadeL_.processSample(io[0][n]) * g;
        if (numCh > 1)
            io[1][n] = cascadeR_.processSample(io[1][n]) * g;
    }

    passivityGain_ = endGain;

    if (needsDry)
        mixInPlace(io, numCh, numSamp, dryBuffer_.data(), p.mix);
}

float DspBridge::estimatePassivityScalar(const Biquad5* sections, int numSections, double sampleRate) noexcept
{
    if (sections == nullptr || numSections <= 0 || sampleRate <= 0.0)
        return 1.0f;

    const double nyquist = 0.5 * sampleRate;
    const double fMax = std::min(20000.0, nyquist - 1.0);
    if (fMax <= 20.0)
        return 1.0f;

    const int bins = 512;
    const double logMin = std::log(20.0);
    const double logMax = std::log(fMax);

    double maxMag = kPassivityEps;
    for (int i = 0; i < bins; ++i)
    {
        const double a = static_cast<double>(i) / static_cast<double>(bins - 1);
        const double freq = std::exp(logMin + a * (logMax - logMin));
        const double w = kTwoPi * freq / sampleRate;

        const std::complex<double> e1 = std::polar(1.0, -w);
        const std::complex<double> e2 = std::polar(1.0, -2.0 * w);

        std::complex<double> H { 1.0, 0.0 };
        for (int s = 0; s < numSections; ++s)
        {
            const auto& sec = sections[s];
            const std::complex<double> num = static_cast<double>(sec.b0) + static_cast<double>(sec.b1) * e1 + static_cast<double>(sec.b2) * e2;
            const std::complex<double> den = 1.0 + static_cast<double>(sec.a1) * e1 + static_cast<double>(sec.a2) * e2;
            H *= num / den;
        }

        maxMag = std::max(maxMag, std::abs(H));
    }

    double scale = kPassivityTarget / std::max(maxMag, kPassivityEps);
    if (!(scale > 0.0) || !std::isfinite(scale))
        return 1.0f;

    scale = std::clamp(scale, kPassivityFloor, 1.0);
    return static_cast<float>(scale);
}

void DspBridge::applyResonanceToSections(Biquad5* sections, int numSections, float resonance) noexcept
{
    applyResonance(sections, numSections, resonance);
}


void DspBridge::mixInPlace(float* const* io, int numCh, int numSamp,
                           const float* dryBuffer, float mixAmount) noexcept
{
    const float wet = std::sin(mixAmount * 1.5707963267948966f);
    const float dry = std::cos(mixAmount * 1.5707963267948966f);

    for (int ch = 0; ch < numCh; ++ch)
    {
        const float* dryPtr = dryBuffer + ch * numSamp;
        for (int n = 0; n < numSamp; ++n)
            io[ch][n] = wet * io[ch][n] + dry * dryPtr[n];
    }
}

