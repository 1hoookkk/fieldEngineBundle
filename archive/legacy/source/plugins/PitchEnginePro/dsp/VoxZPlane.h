#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include "AuthenticEMUZPlane.h"

namespace vox
{

//============================================================
// Utility functions
//============================================================
static inline float dbToLin (float db)  { return std::pow (10.0f, db / 20.0f); }
static inline float linToDb (float lin) { return 20.0f * std::log10 (juce::jmax (lin, 1.0e-12f)); }
static inline float clamp01 (float x)   { return juce::jlimit (0.0f, 1.0f, x); }
static inline float sigmoid (float x)   { return 1.0f / (1.0f + std::exp (-x)); }
static inline float softClip (float x)  { return std::tanh (x); }

//============================================================
// Mid/Side utility
//============================================================
struct MidSide
{
    static inline void toMS (float& L, float& R)
    {
        float M = 0.5f * (L + R);
        float S = 0.5f * (L - R);
        L = M; R = S;
    }
    static inline void toLR (float& M, float& S)
    {
        float L = M + S;
        float R = M - S;
        M = L; S = R;
    }
};

//============================================================
// 3-band envelope tracker (L/M/H) for spectral analysis
//============================================================
struct EnvelopeBands
{
    void prepare (double sr)
    {
        sampleRate = sr;
        setupBand (bLow,  400.0,  1.0);
        setupBand (bMid, 1400.0,  1.0);
        setupBand (bHigh, 3500.0, 1.0);
        reset();
    }

    void reset()
    {
        for (auto* s : { &sL, &sM, &sH }) { s->x1 = s->x2 = s->y1 = s->y2 = 0.0f; }
        envL = envM = envH = 1.0e-6f;
    }

    void processBlock (const float* x, int n)
    {
        for (int i=0; i<n; ++i)
        {
            float yl = tick (bLow,  sL,  x[i]);
            float ym = tick (bMid,  sM,  x[i]);
            float yh = tick (bHigh, sH,  x[i]);

            envL = juce::jmax (yl*yl, envL * 0.995f);
            envM = juce::jmax (ym*ym, envM * 0.995f);
            envH = juce::jmax (yh*yh, envH * 0.995f);
        }
    }

    float dBL() const { return linToDb (envL + 1e-12f); }
    float dBM() const { return linToDb (envM + 1e-12f); }
    float dBH() const { return linToDb (envH + 1e-12f); }

private:
    struct Biquad { float b0{}, b1{}, b2{}, a1{}, a2{}; };
    struct State  { float x1{}, x2{}, y1{}, y2{}; };

    void setupBand (Biquad& b, double fc, double Q)
    {
        double w0 = juce::MathConstants<double>::twoPi * fc / sampleRate;
        double alpha = std::sin (w0) / (2.0 * Q);
        double cosw0 = std::cos (w0);
        double b0 =    alpha;
        double b1 =    0.0;
        double b2 =   -alpha;
        double a0 =    1.0 + alpha;
        double a1 =   -2.0 * cosw0;
        double a2 =    1.0 - alpha;

        b.b0 = (float) (b0 / a0);
        b.b1 = (float) (b1 / a0);
        b.b2 = (float) (b2 / a0);
        b.a1 = (float) (a1 / a0);
        b.a2 = (float) (a2 / a0);
    }

    static inline float tick (const Biquad& b, State& s, float x)
    {
        float y = b.b0 * x + b.b1 * s.x1 + b.b2 * s.x2 - b.a1 * s.y1 - b.a2 * s.y2;
        s.x2 = s.x1; s.x1 = x; s.y2 = s.y1; s.y1 = y;
        return y;
    }

    double sampleRate = 48000.0;
    Biquad bLow, bMid, bHigh;
    State  sL, sM, sH;
    float envL=0, envM=0, envH=0;
};

//============================================================
// Enhanced sibilant detection using HF energy + zero-crossing rate
//============================================================
struct SibilantGuard
{
    void prepare (double sr)
    {
        sampleRate = sr;
        hp.setCoefficients (juce::IIRCoefficients::makeHighPass (sr, 5500.0));
        reset();
    }

    void reset()
    {
        zcrCount = 0; zcrN = 0;
        highRMS = 1e-9f; fullRMS = 1e-9f;
        hp.reset();
    }

    void pushBlock (const float* x, int n)
    {
        for (int i=0; i<n; ++i)
        {
            float y = hp.processSingleSampleRaw (x[i]);
            highRMS = 0.995f * highRMS + 0.005f * (y*y);
            fullRMS = 0.995f * fullRMS + 0.005f * (x[i]*x[i]);

            if (i>0)
            {
                if ((x[i] > 0 && prev <= 0) || (x[i] < 0 && prev >= 0))
                    zcrCount++;
            }
            prev = x[i];
            zcrN++;
        }
    }

    bool isSibilant() const
    {
        float zcr = (float) zcrCount / (float) juce::jmax (1, zcrN);
        float ratio = highRMS / juce::jmax (fullRMS, 1e-12f);
        return (ratio > 0.35f && zcr > 0.12f);
    }

    void endBlock()
    {
        zcrCount = 0; zcrN = 0;
    }

private:
    double sampleRate = 48000.0;
    juce::IIRFilter hp;
    float prev = 0.0f;
    int zcrCount = 0, zcrN = 0;
    float highRMS = 0, fullRMS = 0;
};

//============================================================
// YIN-based pitch detector
//============================================================
struct PitchDetector
{
    void prepare (double sr, int maxBlock)
    {
        sampleRate = sr;
        int maxLag = (int) std::floor (sr / 60.0);
        diff.resize (maxLag+2, 0.0f);
        cmnd.resize (maxLag+2, 0.0f);
        window.resize ((size_t) juce::jlimit (1024, 8192, juce::nextPowerOfTwo (maxBlock * 2)), 0.0f);
    }

    float detect (const float* x, int n)
    {
        if (n < 256) return lastHz;
        const int maxTau = (int) juce::jlimit (int(sampleRate/1000.0), int(sampleRate/60.0), n-1);
        const int minTau = (int) juce::jlimit (int(sampleRate/1000.0), int(sampleRate/1200.0), 8);

        // Difference function
        for (int tau=1; tau<=maxTau; ++tau)
        {
            double sum=0.0;
            for (int i=0; i<n - tau; ++i)
            {
                const float d = x[i] - x[i+tau];
                sum += d * d;
            }
            diff[tau] = (float) sum;
        }

        // Cumulative mean normalized difference
        cmnd[0] = 1.0f;
        float running = 0.0f;
        for (int tau=1; tau<=maxTau; ++tau)
        {
            running += diff[tau];
            cmnd[tau] = diff[tau] * (float) tau / juce::jmax (1.0e-12f, running);
        }

        // Find minimum below threshold
        const float thresh = 0.12f;
        int tau = -1;
        for (int t=minTau; t<=maxTau; ++t)
            if (cmnd[t] < thresh && cmnd[t] == juce::jmin (cmnd[t-1], cmnd[t], cmnd[t+1]))
            { tau = t; break; }

        if (tau < 0) { voiced = false; return lastHz = -1.0f; }

        // Parabolic interpolation
        float tauM1 = (float) cmnd[tau-1], tau0 = (float) cmnd[tau], tauP1 = (float) cmnd[tau+1];
        float denom = 2.0f * (2.0f * tau0 - tauP1 - tauM1);
        float delta = juce::approximatelyEqual (denom, 0.0f) ? 0.0f : (tauP1 - tauM1) / denom;
        float tauF = (float) tau + delta;

        float hz = (float) sampleRate / tauF;
        voiced = true;
        return lastHz = hz;
    }

    bool isVoiced() const { return voiced; }
    float getLastHz() const { return lastHz; }

private:
    double sampleRate = 48000.0;
    std::vector<float> diff, cmnd, window;
    bool voiced = false;
    float lastHz = -1.0f;
};

//============================================================
// Pitch corrector with semitone snapping
//============================================================
struct PitchCorrector
{
    void prepare (double sr)
    {
        sampleRate = sr;
        setRetuneMs (50.0f);
    }

    void setRetuneMs (float ms)
    {
        float T = juce::jmax (1.0f, ms);
        alpha = std::exp (-1.0f / ((T / 1000.0f) * (float) sampleRate));
    }

    float getRatio (float inputHz)
    {
        if (inputHz <= 0.0f) {
            smoothedHz = alpha * smoothedHz + (1-alpha) * 0.0f;
            return 1.0f;
        }

        float midi = 69.0f + 12.0f * std::log2 (inputHz / 440.0f);
        float targetMidi = std::round (midi);
        float targetHz = 440.0f * std::pow (2.0f, (targetMidi - 69.0f) / 12.0f);

        smoothedHz = alpha * smoothedHz + (1.0f - alpha) * targetHz;
        smoothedHz = juce::jlimit (30.0f, 2000.0f, smoothedHz);

        float ratio = (smoothedHz > 0.0f) ? (smoothedHz / inputHz) : 1.0f;
        ratio = juce::jlimit (0.5f, 2.0f, ratio);
        lastRatio = ratio;
        return ratio;
    }

    float getLastSemitoneShift() const
    {
        return 12.0f * std::log2 (juce::jlimit (1.0e-6f, 1.0e6f, lastRatio));
    }

private:
    double sampleRate = 48000.0;
    float alpha = 0.98f;
    float smoothedHz = 0.0f;
    float lastRatio = 1.0f;
};

//============================================================
// Z-Plane provider interface for AuthenticEMUZPlane integration
//============================================================
struct IZPlaneProvider
{
    virtual ~IZPlaneProvider() = default;
    virtual void updateParameters (float morph01, float intensity01, int style) = 0;
    virtual void processBlock (juce::AudioBuffer<float>& buffer) = 0;
    virtual void reset() = 0;
};

//============================================================
// AuthenticEMU Z-Plane provider implementation
//============================================================
struct AuthenticEMUProvider : IZPlaneProvider
{
    AuthenticEMUProvider() = default;

    void prepare(double sampleRate, AuthenticEMUZPlane* emuInstance)
    {
        emu = emuInstance;
        if (emu) {
            emu->prepare(sampleRate);
        }
    }

    void updateParameters (float morph01, float intensity01, int style) override
    {
        if (!emu) return;

        // Map style to EMU shape pairs
        AuthenticEMUZPlane::Shape shapeA, shapeB;
        switch (style)
        {
            case 0: // Air - bright, airy formants
                shapeA = AuthenticEMUZPlane::Shape::VowelAe_Bright;
                shapeB = AuthenticEMUZPlane::Shape::LeadBright;
                break;
            case 2: // Velvet - warm, round formants
                shapeA = AuthenticEMUZPlane::Shape::VowelOh_Round;
                shapeB = AuthenticEMUZPlane::Shape::LeadWarm;
                break;
            default: // Focus - neutral, clear formants
                shapeA = AuthenticEMUZPlane::Shape::VowelEh_Mid;
                shapeB = AuthenticEMUZPlane::Shape::FormantSweep;
                break;
        }

        emu->setShapePair(shapeA, shapeB);
        emu->setMorphPosition(morph01);
        emu->setIntensity(intensity01);
    }

    void processBlock (juce::AudioBuffer<float>& buffer) override
    {
        if (emu) {
            emu->process(buffer);
        }
    }

    void reset() override
    {
        // AuthenticEMUZPlane handles reset internally
    }

private:
    AuthenticEMUZPlane* emu = nullptr; // Reference to processor's EMU instance
};

//============================================================
// Track shifter using modulated delay (zero-latency)
//============================================================
struct TrackShifter
{
    void prepare (double sr, int channels)
    {
        sampleRate = sr;
        const int maxDelay = (int) std::round (0.030 * sr);
        for (int ch=0; ch<channels; ++ch)
        {
            delay.emplace_back();
            auto& d = delay.back();
            d.resize ((size_t) juce::jmax (2048, maxDelay*2), 0.0f);
            wp.push_back (0);
            readPhase.push_back (0.0f);
        }
    }

    void reset()
    {
        for (auto& d : delay) std::fill (d.begin(), d.end(), 0.0f);
        std::fill (wp.begin(), wp.end(), 0);
        std::fill (readPhase.begin(), readPhase.end(), 0.0f);
        smoothedRatio = 1.0f;
    }

    void setRatio (float r) { targetRatio = juce::jlimit (0.5f, 2.0f, r); }

    void process (juce::AudioBuffer<float>& buf)
    {
        const int numCh = buf.getNumChannels();
        const int n = buf.getNumSamples();

        float a = std::exp (-1.0f / (0.002f * (float) sampleRate));
        for (int ch=0; ch<numCh; ++ch)
        {
            auto* x  = buf.getWritePointer (ch);
            auto& d  = delay[(size_t) ch];
            int   w  = wp[(size_t) ch];
            float ph = readPhase[(size_t) ch];

            for (int i=0; i<n; ++i)
            {
                d[(size_t) w] = x[i];
                smoothedRatio = a * smoothedRatio + (1.0f - a) * targetRatio;

                float readOffset = ph;
                float y = hermite4 (d, w, -readOffset);
                x[i] = y;

                ph += (smoothedRatio - 1.0f) * 0.5f;
                if (ph > maxPhase) ph -= maxPhase;
                if (ph < 0.0f)     ph += maxPhase;

                w = (w + 1) % (int) d.size();
            }

            wp[(size_t) ch] = w;
            readPhase[(size_t) ch] = ph;
        }
    }

private:
    static inline float hermite4 (const std::vector<float>& b, int w, float offset)
    {
        int size = (int) b.size();
        float pos = (float) w + offset;
        while (pos < 0.0f) pos += (float) size;
        while (pos >= (float) size) pos -= (float) size;

        int i1 = (int) std::floor (pos);
        float t = pos - (float) i1;
        int i0 = (i1 - 1 + size) % size;
        int i2 = (i1 + 1) % size;
        int i3 = (i1 + 2) % size;

        float y0 = b[(size_t) i0], y1 = b[(size_t) i1], y2 = b[(size_t) i2], y3 = b[(size_t) i3];

        float c0 = y1;
        float c1 = 0.5f * (y2 - y0);
        float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);

        return ((c3 * t + c2) * t + c1) * t + c0;
    }

    double sampleRate = 48000.0;
    std::vector<std::vector<float>> delay;
    std::vector<int>   wp;
    std::vector<float> readPhase;
    float targetRatio = 1.0f;
    float smoothedRatio = 1.0f;
    const float maxPhase = 512.0f;
};

//============================================================
// Variable-rate shifter with lookahead (Print mode)
//============================================================
struct PrintShifter
{
    void prepare (double sr, int maxBlock, int channels)
    {
        sampleRate = sr;
        lookAheadSamples = (int) std::round (0.010 * sr); // 10 ms
        for (int ch=0; ch<channels; ++ch)
        {
            buffers.emplace_back();
            auto& b = buffers.back();
            b.resize ((size_t) juce::jmax (8192, maxBlock*4 + lookAheadSamples + 1024), 0.0f);
            wpos.push_back (0);
            rpos.push_back (0.0f);
        }
    }

    void reset()
    {
        for (auto& b : buffers) std::fill (b.begin(), b.end(), 0.0f);
        std::fill (wpos.begin(), wpos.end(), 0);
        std::fill (rpos.begin(), rpos.end(), 0.0f);
    }

    void setRatio (float r) { ratio = juce::jlimit (0.5f, 2.0f, r); }

    void process (juce::AudioBuffer<float>& buf)
    {
        const int numCh = buf.getNumChannels();
        const int n = buf.getNumSamples();

        // Write with lookahead
        for (int ch=0; ch<numCh; ++ch)
        {
            auto* in  = buf.getReadPointer (ch);
            auto& b   = buffers[(size_t) ch];
            int  wp   = wpos[(size_t) ch];

            for (int i=0; i<n; ++i)
            {
                b[(size_t) ((wp + i) % b.size())] = in[i];
            }
            wpos[(size_t) ch] = (wp + n) % (int) b.size();
        }

        // Read at different rate with interpolation
        for (int ch=0; ch<numCh; ++ch)
        {
            auto* out = buf.getWritePointer (ch);
            auto& b   = buffers[(size_t) ch];
            float rp  = rpos[(size_t) ch];
            int   wp  = wpos[(size_t) ch];

            for (int i=0; i<n; ++i)
            {
                float readIndex = (float) ((wp - lookAheadSamples + b.size()) % b.size());
                float pos = std::fmod (readIndex + rp + b.size(), (float) b.size());
                out[i] = interpolate (b.data(), (int) b.size(), pos);
                rp += ratio;
            }
            rpos[(size_t) ch] = rp;
        }
    }

private:
    float interpolate (const float* buf, int size, float pos) const
    {
        int ipos = (int) std::floor (pos);
        float frac = pos - (float) ipos;

        // Linear interpolation for simplicity
        int idx1 = juce::jlimit(0, size-1, ipos);
        int idx2 = juce::jlimit(0, size-1, ipos + 1);

        return buf[idx1] * (1.0f - frac) + buf[idx2] * frac;
    }

    double sampleRate = 48000.0;
    std::vector<std::vector<float>> buffers;
    std::vector<int> wpos;
    std::vector<float> rpos;
    int lookAheadSamples = 0;
    float ratio = 1.0f;
};

//============================================================
// Main VoxZPlane brain - orchestrates the entire pipeline
//============================================================
struct Brain
{
    enum Mode { Track = 0, Print = 1 };

    void prepare (double sr, int blockSize, int channels)
    {
        sampleRate = sr;
        numCh = channels;

        envIn.prepare (sr);
        envOut.prepare (sr);
        sibilants.prepare (sr);
        f0.prepare (sr, blockSize);
        corr.prepare (sr);

        // EMU providers will be prepared when wired via wireEMUProviders()

        track.prepare (sr, channels);
        print.prepare (sr, blockSize, channels);

        // Preallocate internal work buffers
        tmpBlock.setSize (1, blockSize, false, true, true);
        zMidBuf.setSize (1, blockSize, false, true, true);
        zSideBuf.setSize (1, blockSize, false, true, true);

        setStyle (1);
        reset();
    }

    void reset()
    {
        envIn.reset();
        envOut.reset();
        sibilants.reset();
        // EMU providers are reset via their underlying EMU instances
        zPre.reset();
        zPostM.reset();
        zPostS.reset();
        track.reset();
        print.reset();
        rRMS = 1.0f;
    }

    void setMode (Mode m) { mode = m; }
    void setStyle (int idx) { style = juce::jlimit (0,2,idx); }
    void setUserMix (float m) { userMix = clamp01 (m); }
    void setDriveBase (float d) { driveBase = juce::jlimit (0.0f, 2.0f, d); }
    void setRetuneMs (float ms) { corr.setRetuneMs (ms); }
    void setBypass (bool b) { bypass = b; }

    // Wire up AuthenticEMUZPlane instances from processor
    void wireEMUProviders(AuthenticEMUZPlane* preEmu, AuthenticEMUZPlane* postMEmu, AuthenticEMUZPlane* postSEmu)
    {
        zPre.prepare(sampleRate, preEmu);
        zPostM.prepare(sampleRate, postMEmu);
        zPostS.prepare(sampleRate, postSEmu);
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        juce::ScopedNoDenormals noDenormals;

        const int n = buffer.getNumSamples();
        if (buffer.getNumChannels() == 1)
            buffer.setSize (2, n, true, true, true);

        // Ensure internal buffers are large enough (host can change block size)
        if (n > tmpBlock.getNumSamples())
        {
            tmpBlock.setSize (1, n, false, true, true);
            zMidBuf.setSize (1, n, false, true, true);
            zSideBuf.setSize (1, n, false, true, true);
        }

        // Convert L/R → M/S
        for (int i=0; i<n; ++i)
        {
            float L = buffer.getReadPointer (0)[i];
            float R = buffer.getReadPointer (1)[i];
            MidSide::toMS (L, R);
            buffer.getWritePointer (0)[i] = L; // M
            buffer.getWritePointer (1)[i] = R; // S
        }

        auto* M = buffer.getWritePointer (0);
        auto* S = buffer.getWritePointer (1);

        // Analysis on Mid channel
        envIn.processBlock (M, n);
        sibilants.pushBlock (M, n);
        float f0Hz = f0.detect (M, n);
        bool voiced = f0.isVoiced();
        float ratio = corr.getRatio (voiced ? f0Hz : -1.0f);
        float semis = corr.getLastSemitoneShift();

        // Pre-emphasis (voiced-only, off on sibilants)
        const bool sib = sibilants.isSibilant();
        if (voiced && !sib)
        {
            zPre.updateParameters(0.45f, 0.2f, style);
            for (int i=0;i<n;++i) M[i] = M[i]; // zPre.processSample would go here
        }

        // Pitch shift
        if (! bypass)
        {
            float rAbs = std::abs (ratio);
            rRMS = 0.9f * rRMS + 0.1f * rAbs;

            if (mode == Track)
            {
                track.setRatio (ratio);
                tmpBlock.copyFrom (0, 0, M, n);
                track.process (tmpBlock);
                std::memcpy (M, tmpBlock.getReadPointer (0), sizeof(float) * (size_t) n);
            }
            else // Print
            {
                print.setRatio (ratio);
                tmpBlock.copyFrom (0, 0, M, n);
                print.process (tmpBlock);
                std::memcpy (M, tmpBlock.getReadPointer (0), sizeof(float) * (size_t) n);
            }
        }

        // Envelope analysis for adaptive mapping
        envOut.processBlock (M, n);
        float dL = envOut.dBL() - envIn.dBL();
        float dM = envOut.dBM() - envIn.dBM();
        float dH = envOut.dBH() - envIn.dBH();

        // Adaptive parameter mapping
        const float s = semis;
        const float k1 = 0.035f, k2 = 0.020f;
        const float b0 = 0.45f, b1 = 0.04f, b2 = 0.02f;
        const float d1 = 0.05f;

        float morph = clamp01 (0.5f + k1 * s + k2 * (dH - dL));
        float intensity = juce::jlimit (0.2f, 1.0f, b0 + b1 * std::abs (s) + b2 * juce::jmax (0.0f, -dM));
        float drive = juce::jlimit (0.0f, 1.5f, driveBase + d1 * std::abs (s));

        // Style adjustments
        if (style == 0) { morph = clamp01 (morph + 0.08f); intensity *= 0.88f; }
        if (style == 2) { morph = clamp01 (morph - 0.08f); intensity = juce::jlimit (0.2f, 1.25f, intensity * 1.15f); }

        // Sibilant guard
        if (sib)
        {
            intensity *= 0.25f;
            intensity = juce::jmin (intensity, 0.35f);
        }

        // Post-shift Z-plane processing (using providers)
        zPostM.updateParameters(morph, intensity, style);
        zPostS.updateParameters(morph, juce::jlimit (0.0f, 1.0f, intensity * 0.4f), style);

        // Apply Z-plane processing using preallocated buffers
        zMidBuf.copyFrom(0, 0, M, n);
        zSideBuf.copyFrom(0, 0, S, n);

        zPostM.processBlock(zMidBuf);
        zPostS.processBlock(zSideBuf);

        std::memcpy(M, zMidBuf.getReadPointer(0), sizeof(float) * (size_t) n);
        std::memcpy(S, zSideBuf.getReadPointer(0), sizeof(float) * (size_t) n);

        // Mix control
        if (! bypass)
        {
            float wet = userMix, dry = 1.0f - wet;
            for (int i=0;i<n;++i) {
                M[i] = wet * M[i] + dry * 0.0f; // Would need dry signal storage
                S[i] = wet * S[i];
            }
        }

        // Convert back M/S → L/R
        for (int i=0; i<n; ++i)
        {
            float m = buffer.getReadPointer (0)[i];
            float s_ = buffer.getReadPointer (1)[i];
            MidSide::toLR (m, s_);
            buffer.getWritePointer (0)[i] = m;
            buffer.getWritePointer (1)[i] = s_;
        }

        sibilants.endBlock();
    }

private:
    double sampleRate = 48000.0;
    int numCh = 2;
    Mode mode = Track;
    int style = 1;
    bool bypass = false;

    EnvelopeBands envIn, envOut;
    SibilantGuard sibilants;
    PitchDetector f0;
    PitchCorrector corr;

    AuthenticEMUProvider zPre, zPostM, zPostS;
    TrackShifter track;
    PrintShifter print;

    // Preallocated internal work buffers to avoid per-block allocation
    juce::AudioBuffer<float> tmpBlock; // for shifters
    juce::AudioBuffer<float> zMidBuf;  // post-Z-plane mid
    juce::AudioBuffer<float> zSideBuf; // post-Z-plane side

    float rRMS = 1.0f;
    float userMix = 1.0f;
    float driveBase = 0.1f;
};

} // namespace vox