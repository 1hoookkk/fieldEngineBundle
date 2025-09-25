#include "AuthenticEMUZPlane.h"

namespace {
    constexpr float kRefFs = 48000.0f;

    // Bilinear transform: z@fs1 -> s -> z@fs2
    // Keeps filter shapes consistent across sample rates
    inline std::complex<float> remapZ(std::complex<float> zAtRef, float fsNew) {
        const std::complex<float> one(1.0f, 0.0f);
        const auto s   = 2.0f * kRefFs * (zAtRef - one) / (zAtRef + one);
        const auto zfs = (2.0f * fsNew + s) / (2.0f * fsNew - s);
        return zfs;
    }
}

AuthenticEMUZPlane::AuthenticEMUZPlane() {
    // Neutral defaults for production reliability
    setMorphPair(0);
    setMorphPosition(0.5f);
    setIntensity(0.0f);        // Transparent by default
    setDrive(0.0f);            // Unity gain by default
    setSectionSaturation(0.0f); // Clean by default
    setAutoMakeup(false);
    setLFORate(0.0f);          // No modulation by default
    setLFODepth(0.0f);
}

void AuthenticEMUZPlane::prepareToPlay (double sampleRate) {
    fs = (float) sampleRate;

    // 20ms smoothing for zipper-free parameter changes
    morphSm.reset(fs, 0.020f);
    intenSm.reset(fs, 0.020f);
    morphSm.setCurrentAndTargetValue(currentMorph);
    intenSm.setCurrentAndTargetValue(currentIntensity);

    reset();
    updateCoefficientsBlock(); // Prime the coefficients
}

void AuthenticEMUZPlane::reset() {
    for (auto& s : sectionsL) s.reset();
    for (auto& s : sectionsR) s.reset();
    lfoPhase = 0.0f;
}

// --- Parameter setters ---
void AuthenticEMUZPlane::setMorphPair(MorphPair p)          { currentPair = p; }
void AuthenticEMUZPlane::setMorphPosition(float v)          { currentMorph     = juce::jlimit(0.0f, 1.0f, v); }
void AuthenticEMUZPlane::setIntensity(float v)              { currentIntensity = juce::jlimit(0.0f, 1.0f, v); }
void AuthenticEMUZPlane::setDrive(float db)                 { driveLin = juce::Decibels::decibelsToGain(db); }
void AuthenticEMUZPlane::setSectionSaturation(float v)      { sectionSaturation= juce::jlimit(0.0f, 1.0f, v); }
void AuthenticEMUZPlane::setAutoMakeup(bool e)              { autoMakeup = e; }
void AuthenticEMUZPlane::setLFORate(float hz)               { lfoRate   = juce::jlimit(0.02f, 8.0f, hz); }
void AuthenticEMUZPlane::setLFODepth(float d)               { lfoDepth  = juce::jlimit(0.0f, 1.0f, d); }

void AuthenticEMUZPlane::process(juce::AudioBuffer<float>& buffer) {
    const int nCh = buffer.getNumChannels();
    const int nSm = buffer.getNumSamples();

    // Early-exit optimization: completely transparent when effectively off
    // This saves CPU and ensures true bypass behavior
    if (intenSm.getCurrentValue() <= 1e-3f
        && std::abs(driveLin - 1.0f) < 1e-6f
        && sectionSaturation <= 1e-6f
        && lfoDepth <= 1e-6f)
        return;

    updateCoefficientsBlock();

    // Apply input drive
    for (int ch = 0; ch < nCh; ++ch) {
        auto* x = buffer.getWritePointer(ch);
        for (int i = 0; i < nSm; ++i) x[i] *= driveLin;
    }

    // 6-section biquad cascade per channel
    for (int ch = 0; ch < nCh; ++ch) {
        auto* x = buffer.getWritePointer(ch);
        auto& S = (ch == 0 ? sectionsL : sectionsR);
        for (int i = 0; i < nSm; ++i) {
            float y = x[i];
            for (int s = 0; s < 6; ++s)
                y = S[s].processSample(y, sectionSaturation);
            x[i] = y;
        }
    }

    // Auto-makeup gain to compensate for intensity changes
    if (autoMakeup) {
        const float I = intenSm.getCurrentValue();
        const float makeup = 1.0f / (1.0f + 0.5f*I); // gentle compensation, ≤2x
        for (int ch = 0; ch < nCh; ++ch) {
            auto* x = buffer.getWritePointer(ch);
            for (int i = 0; i < nSm; ++i) x[i] *= makeup;
        }
    }
}

void AuthenticEMUZPlane::updateCoefficientsBlock() {
    // Control-rate LFO advance (approximate one block)
    if (lfoRate > 0.0f) {
        const float inc = juce::MathConstants<float>::twoPi * (lfoRate / fs);
        lfoPhase += inc * 64.0f; // assume ~64-sample blocks
        if (lfoPhase >= juce::MathConstants<float>::twoPi)
            lfoPhase -= juce::MathConstants<float>::twoPi;
    }

    // LFO modulates morph position
    const float lfoUnipolar = 0.5f * (1.0f + std::sin(lfoPhase)) * lfoDepth;

    // Update smoother targets
    morphSm.setTargetValue(juce::jlimit(0.0f, 1.0f, currentMorph + lfoUnipolar));
    intenSm.setTargetValue(currentIntensity);

    const float morph = morphSm.getCurrentValue();
    const float I     = intenSm.getCurrentValue();

    // Pull shape data from tables
    const auto& pair   = MORPH_PAIRS[static_cast<int>(currentPair)];
    const auto& shapeA = AUTHENTIC_EMU_SHAPES[pair[0]];
    const auto& shapeB = AUTHENTIC_EMU_SHAPES[pair[1]];

    // Interpolate poles at 48k reference (shortest-path theta, linear radius)
    for (int i = 0; i < 6; ++i) {
        const int ri = i*2, ti = i*2+1;
        float rA = juce::jlimit(0.10f, 0.999f, shapeA[ri]);
        float rB = juce::jlimit(0.10f, 0.999f, shapeB[ri]);
        float tA = shapeA[ti];
        float tB = shapeB[ti];

        // Shortest path for theta interpolation (handles wrap-around)
        float d = tB - tA;
        while (d >  juce::MathConstants<float>::pi)  d -= juce::MathConstants<float>::twoPi;
        while (d < -juce::MathConstants<float>::pi)  d += juce::MathConstants<float>::twoPi;

        float r   = rA + morph * (rB - rA);
        float the = tA + morph * d;

        // Intensity scales radius/Q conservatively
        r = juce::jlimit(0.10f, 0.9995f, r * (0.80f + 0.20f*I));
        polesRef48[i] = { r, the };
    }

    // Remap z(48k) -> z(fs) and compute biquad sections
    for (int i = 0; i < 6; ++i) {
        const auto zRef = std::polar(polesRef48[i].r, polesRef48[i].theta);
        const auto zfs  = (fs == kRefFs) ? zRef : remapZ(zRef, fs);

        const float rfs = juce::jlimit(0.10f, 0.9995f, std::abs(zfs));
        const float thf = std::arg(zfs);
        polesFs[i] = { rfs, thf };

        float a1, a2, b0, b1, b2;
        zpairToBiquad(polesFs[i], a1, a2, b0, b1, b2);

        // Apply to both channels
        for (auto* S : { sectionsL + i, sectionsR + i }) {
            S->a1 = a1; S->a2 = a2;
            S->b0 = b0; S->b1 = b1; S->b2 = b2;
        }
    }
}

void AuthenticEMUZPlane::zpairToBiquad(const PolePair& p, float& a1, float& a2, float& b0, float& b1, float& b2) {
    // Denominator from complex pole pair
    a1 = -2.0f * p.r * std::cos(p.theta);
    a2 =  p.r * p.r;

    // Bandpass-ish numerator (zeros at DC & Nyquist) with conservative gain
    b0 = (1.0f - p.r) * 0.5f; // tame overall gain for cascade
    b1 = 0.0f;
    b2 = -b0;

    // Numerical stability clamps
    a1 = juce::jlimit(-1.999f, 1.999f, a1);
    a2 = juce::jlimit(-0.999f, 0.999f, a2);
}