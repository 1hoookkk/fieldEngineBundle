// HarmonicQuantizer.h
// Header-only, RT-safe helpers for harmonic quantization / snap weighting.
// Implements computeSnapWeight(freq) and pressure->sigma mapping.
// No allocations, no locks. Suitable for calling in processBlock() or
// inside SpectralSynthEngine voice creation routines.
#pragma once

// Undefine Windows max macro to prevent conflicts with std::max
#ifdef max
#undef max
#endif

#include <array>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <limits>

namespace scp { // SpectralCanvas Pro namespace

// --- Configuration: C Major pitch-classes (MVP) ---
inline constexpr std::array<int,7> C_MAJOR_PCS = {0,2,4,5,7,9,11};

// --- Math helpers (fast, RT-safe) ---
inline double clamp01(double v) noexcept { return (v < 0.0) ? 0.0 : (v > 1.0 ? 1.0 : v); }

inline double freqToMidiDouble(double f) noexcept {
    // protect against non-positive freq
    if (!(f > 0.0)) return 0.0;
    return 69.0 + 12.0 * std::log2(f / 440.0);
}

inline double midiToFreqDouble(double m) noexcept {
    return 440.0 * std::pow(2.0, (m - 69.0) / 12.0);
}

// Map brush pressure ∈ [0..1] to sigma in cents.
// Higher pressure -> smaller sigma -> stronger snap.
inline double pressureToSigmaCents(double pressure,
                                   double sigmaMax = 200.0,
                                   double sigmaMin = 8.0) noexcept
{
    double p = clamp01(pressure);
    // linear interpolation (you can switch to an exponential curve if desired)
    return (1.0 - p) * sigmaMax + p * sigmaMin;
}

// Find the nearest MIDI note (integer) for a given MIDI float that has given pitch-class.
// Searches +/- 6 semitones around base to avoid wide searches; deterministic and cheap.
inline int nearestMidiForPitchClass(double midiFloat, int pitchClass) noexcept
{
    int base = int(std::lround(midiFloat));
    int best = base;
    int bestDist = std::numeric_limits<int>::max();
    // search range: +/- 6 semitones (one tritone each way) -- adjustable
    for (int k = -6; k <= 6; ++k) {
        int cand = base + k;
        int candPc = ((cand % 12) + 12) % 12;
        if (candPc == pitchClass) {
            int dist = std::abs(cand - base);
            if (dist < bestDist) { bestDist = dist; best = cand; }
        }
    }
    return best;
}

// Compute snap weight for a single frequency (Hz) given a set of pitch-classes and sigma (cents).
// Returns w in [0..1] where 1=full snap (exact match), 0=no snap influence.
inline double computeSnapWeight(double frequencyHz,
                                const std::array<int,7>& scalePCs,
                                double sigmaCents) noexcept
{
    if (!(frequencyHz > 0.0)) return 0.0;
    // avoid division by zero
    const double eps = 1e-12;
    double midiF = freqToMidiDouble(frequencyHz);

    double bestAbsCents = std::numeric_limits<double>::infinity();
    for (int pc : scalePCs) {
        int tgtMidi = nearestMidiForPitchClass(midiF, pc);
        double dCents = (midiF - double(tgtMidi)) * 100.0; // difference in cents
        double absCents = std::abs(dCents);
        if (absCents < bestAbsCents) bestAbsCents = absCents;
    }

    // clamp sigma
    double sigma = (sigmaCents > eps) ? sigmaCents : eps;
    // Gaussian weight based on cents distance
    double exponent = - (bestAbsCents * bestAbsCents) / (2.0 * sigma * sigma);
    // protect from underflow/overflow
    if (exponent < -700.0) return 0.0;
    double w = std::exp(exponent);
    // Optional: clamp to [0,1]
    if (w < 0.0) w = 0.0;
    if (w > 1.0) w = 1.0;
    return w;
}

// Convenience overload: use the default C major set
inline double computeSnapWeightCmaj(double frequencyHz, double sigmaCents) noexcept {
    return computeSnapWeight(frequencyHz, C_MAJOR_PCS, sigmaCents);
}

// --- Additional helpers for frequency-blend variant ---
// Compute nearest target MIDI for the given frequency restricted to the scale pitch-classes.
inline int computeNearestTargetMidiForScale(double frequencyHz,
                                           const std::array<int,7>& scalePCs) noexcept
{
    double midiF = freqToMidiDouble(frequencyHz);
    // find best candidate across all pitch-classes
    double bestAbsCents = std::numeric_limits<double>::infinity();
    int bestMidi = int(std::lround(midiF));
    for (int pc : scalePCs) {
        int tgt = nearestMidiForPitchClass(midiF, pc);
        double dCents = std::abs((midiF - double(tgt)) * 100.0);
        if (dCents < bestAbsCents) {
            bestAbsCents = dCents;
            bestMidi = tgt;
        }
    }
    return bestMidi;
}

// Frequency blending helper: linearly blend from originalFreq -> targetFreq by weight w [0..1]
inline double blendFrequency(double originalFreq, double targetFreq, double w) noexcept
{
    // simple linear blend is cheap and RT-safe. If you want an alternative perceptual blend,
    // swap this with a pitch-domain crossfade in MIDI space.
    if (!(originalFreq > 0.0) || !(targetFreq > 0.0)) return originalFreq;
    return (1.0 - w) * originalFreq + w * targetFreq;
}

// Convenience: compute target freq for scale using C major
inline double computeSnappedFrequencyCmaj(double frequencyHz, double sigmaCents, double &outWeightCents) noexcept
{
    const double DELTA_MAX_CENTS = 600.0; // clamp to +/- 6 semitones
    int tgtMidi = computeNearestTargetMidiForScale(frequencyHz, C_MAJOR_PCS);
    double midiF = freqToMidiDouble(frequencyHz);
    double dCents = (midiF - double(tgtMidi)) * 100.0;
    // clamp
    if (dCents > DELTA_MAX_CENTS) dCents = DELTA_MAX_CENTS;
    if (dCents < -DELTA_MAX_CENTS) dCents = -DELTA_MAX_CENTS;
    outWeightCents = std::abs(dCents);
    double w = computeSnapWeight(frequencyHz, C_MAJOR_PCS, sigmaCents);
    double targetFreq = midiToFreqDouble(double(tgtMidi));
    return blendFrequency(frequencyHz, targetFreq, w);
}

} // namespace scp