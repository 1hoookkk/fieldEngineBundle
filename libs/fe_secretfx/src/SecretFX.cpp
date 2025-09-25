// Implementation of fe::secretfx::Engine
#include "fe/secretfx/SecretFX.h"
#include <cmath>
#include <complex>
#include "BinaryData.h" // from fe_secretfx_data (ensures data is linked)

namespace fe { namespace secretfx {

void Engine::prepare(double sampleRate, int /*samplesPerBlock*/, int numChannels) noexcept {
    fs_ = static_cast<float>(sampleRate);
    ch_ = juce::jlimit(1, 2, numChannels);
    reset();
    makeup_.reset(fs_, 0.05);
    makeup_.setCurrentAndTargetValue(1.0f);

    // Load embedded pair shapes (A&B) once
    loadEmbeddedPairsIfNeeded();
}

void Engine::reset() noexcept {
    for (auto& s : L_) s.reset();
    for (auto& s : R_) s.reset();
    preRMS_ = postRMS_ = 1e-6f;
}

inline float Engine::wrapShortest(float a0, float a1) noexcept {
    float d = a1 - a0;
    const float pi = juce::MathConstants<float>::pi;
    while (d >  pi) d -= juce::MathConstants<float>::twoPi;
    while (d < -pi) d += juce::MathConstants<float>::twoPi;
    return d; // return shortest delta, not absolute angle
}

inline Engine::Pole Engine::interpPole(const Pole& a, const Pole& b, float t, float intensity01) noexcept {
    Pole out;
    const float ra = juce::jlimit(0.10f, 0.995f, a.r);
    const float rb = juce::jlimit(0.10f, 0.995f, b.r);
    out.r = ra + t * (rb - ra);
    const float dth = wrapShortest(a.theta, b.theta);
    out.theta = a.theta + t * dth;
    // conservative Q scale
    const float q = 0.85f + juce::jlimit(0.0f, 1.0f, intensity01) * 0.12f;
    out.r = juce::jlimit(0.10f, 0.998f, out.r * q);
    return out;
}

inline void Engine::poleToBiquad(const Pole& p, Biquad& s) noexcept {
    const float r  = juce::jlimit(0.10f, 0.998f, p.r);
    const float th = p.theta;
    const float a1 = -2.0f * r * std::cos(th);
    const float a2 = r * r;
    // Band-pass-ish numerator: DC/Nyquist zeros
    float b0 = (1.0f - a2) * 0.5f;
    const float b1 = 0.0f;
    float b2 = -b0;
    // Clamp for stability
    s.a1 = juce::jlimit(-1.98f, 1.98f, a1);
    s.a2 = juce::jlimit(-0.98f, 0.98f, a2);
    s.b0 = juce::jlimit(-2.0f, 2.0f, b0);
    s.b1 = b1;
    s.b2 = juce::jlimit(-2.0f, 2.0f, b2);
}

void Engine::updateCoeffs(int numSamples) noexcept {
    // LFO advance using real block size
    const float inc = juce::MathConstants<float>::twoPi * (snap_.lfoRateHz / fs_);
    lfoPhase_ += inc * static_cast<float>(numSamples);
    if (lfoPhase_ >= juce::MathConstants<float>::twoPi)
        lfoPhase_ -= juce::MathConstants<float>::twoPi;

    const float lfoU = 0.5f * (1.0f + std::sin(lfoPhase_));
    const float morph = juce::jlimit(0.0f, 1.0f, snap_.morph01 + snap_.lfoDepth01 * lfoU);

    // Pick default pair (vowel_pair) or first available
    auto* pair = pairs_.contains(defaultPair_) ? pairs_.getPointer(defaultPair_) : nullptr;
    if (pair == nullptr)
    {
        // If no embedded shapes loaded, synthesize mild shapes as a fallback
        for (int i = 0; i < kSections; ++i)
        {
            const float rA = 0.93f + 0.01f * i;
            const float rB = 0.90f + 0.02f * i;
            const float tA = 0.12f + 0.18f * i;
            const float tB = 0.10f + 0.20f * i;
            Pole a{ rA, tA }, b{ rB, tB };
            poles48_[static_cast<size_t>(i)] = interpPole(a, b, morph, snap_.intensity01);
        }
    }
    else
    {
        for (int i = 0; i < kSections; ++i)
        {
            poles48_[static_cast<size_t>(i)] = interpPole(pair->A[(size_t)i], pair->B[(size_t)i], morph, snap_.intensity01);
        }
    }

    // Map z@48k-ish reference to current fs via bilinear transform
    auto remapZ = [&](const Pole& pref) {
        std::complex<float> zRef = std::polar(pref.r, pref.theta);
        const std::complex<float> one(1.0f, 0.0f);
        constexpr float kRefFs = 48000.0f;
        const auto s   = 2.0f * kRefFs * (zRef - one) / (zRef + one);
        const auto zfs = (2.0f * fs_ + s) / (2.0f * fs_ - s);
        Pole out;
        out.r = juce::jlimit(0.10f, 0.998f, std::abs(zfs));
        out.theta = std::arg(zfs);
        return out;
    };

    for (int i = 0; i < kSections; ++i) {
        const auto pfs = remapZ(poles48_[static_cast<size_t>(i)]);
        poleToBiquad(pfs, L_[static_cast<size_t>(i)]);
        poleToBiquad(pfs, R_[static_cast<size_t>(i)]);
    }
}

void Engine::loadEmbeddedPairsIfNeeded() noexcept {
    if (!pairs_.isEmpty()) return;

    auto load = [&](const char* resName)->juce::var {
        int size = 0;
        if (auto* data = SecretFXData::getNamedResource(resName, size))
        {
            juce::String s(juce::CharPointer_UTF8(data), (size_t) size);
            return juce::JSON::parse(s);
        }
        return {};
    };

    auto varA = load("audity_shapes_A_48k_json");
    auto varB = load("audity_shapes_B_48k_json");
    if (!varA.isObject() || !varB.isObject()) return;

    auto parseTable = [&](const juce::var& root, juce::HashMap<juce::String, std::array<Pole, kSections>>& out){
        auto shapes = root.getProperty("shapes", juce::var());
        if (!shapes.isArray()) return;
        for (const auto& v : *shapes.getArray())
        {
            auto id = v.getProperty("id", juce::var());
            if (!id.isString()) continue;
            auto poles = v.getProperty("poles", juce::var());
            if (!poles.isArray()) continue;
            std::array<Pole, kSections> arr{};
            auto* a = poles.getArray();
            for (int i = 0; i < kSections && i < a->size(); ++i)
            {
                const auto& pv = (*a)[i];
                arr[(size_t)i] = Pole{ (float) pv.getProperty("r", 0.95f),
                                       (float) pv.getProperty("theta", 0.0f) };
            }
            out.set(id.toString(), arr);
        }
    };

    juce::HashMap<juce::String, std::array<Pole, kSections>> A, B;
    parseTable(varA, A);
    parseTable(varB, B);

    for (auto it = A.begin(); it != A.end(); ++it)
    {
        const juce::String key = it.getKey();
        if (auto* pb = B.getPointer(key))
        {
            PairShapes p; p.A = it.getValue(); p.B = *pb; pairs_.set(key, p);
        }
    }
    // Ensure default exists if possible
    if (!pairs_.contains(defaultPair_))
    {
        for (auto it = pairs_.begin(); it != pairs_.end(); ++it) { defaultPair_ = it.getKey(); break; }
    }
}

void Engine::process(juce::AudioBuffer<float>& wet) noexcept {
    const int nCh = juce::jmin(ch_, wet.getNumChannels());
    const int nSm = wet.getNumSamples();
    if (nCh <= 0 || nSm <= 0) return;

    updateCoeffs(nSm);

    // Apply drive
    const float drive = juce::Decibels::decibelsToGain(snap_.driveDb);
    for (int ch = 0; ch < nCh; ++ch) {
        auto* x = wet.getWritePointer(ch);
        for (int i = 0; i < nSm; ++i) x[i] *= drive;
    }

    // Process cascade per channel
    preRMS_ = 1e-6f; postRMS_ = 1e-6f;
    auto updRms = [&](float v, float& s){
        const float tau = 0.05f; // quick-ish RMS
        const float a = 1.0f - std::exp(-1.0f / (tau * fs_));
        s = (1.0f - a) * s + a * (v*v);
    };

    for (int n = 0; n < nSm; ++n) {
        for (int ch = 0; ch < nCh; ++ch) {
            auto& S = (ch == 0 ? L_ : R_);
            float v = wet.getWritePointer(ch)[n];
            updRms(v, preRMS_);
            for (int s = 0; s < kSections; ++s)
                v = S[static_cast<size_t>(s)].tick(v, snap_.sectionSat01);
            wet.getWritePointer(ch)[n] = v;
            updRms(v, postRMS_);
        }
    }

    // Auto makeup
    if (snap_.autoMakeup) {
        const float pre  = std::sqrt(juce::jmax(preRMS_, 1e-12f));
        const float post = std::sqrt(juce::jmax(postRMS_, 1e-12f));
        float corr = post > 1e-6f ? pre / post : 1.0f;
        corr = juce::jlimit(0.5f, 2.0f, corr);
        makeup_.setTargetValue(corr);
        for (int i = 0; i < nSm; ++i) {
            const float g = makeup_.getNextValue();
            for (int ch = 0; ch < nCh; ++ch) {
                wet.getWritePointer(ch)[i] *= g;
            }
        }
    }
}

}} // namespace fe::secretfx
