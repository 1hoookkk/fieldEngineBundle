// PitchEngine.h
#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct PitchBlock {
    const float* ratio = nullptr; // length N; 1.0 = no shift
    bool voiced = false;
    bool sibilant = false;
    float f0 = 0.0f;      // smoothed Hz (0 => unvoiced)
    int N = 0;
};

class PitchEngine {
public:
    void prepare(double fs, int maxBlock, float fMin=70.f, float fMax=800.f) {
        sr = fs; Nmax = std::max(256, maxBlock);
        frameSz = 1024; hop = 256;
        setRange(fMin, fMax);
        frame.assign((size_t)frameSz, 0.0f);
        filt.assign((size_t)Nmax, 0.0f);
        ratioBuf.assign((size_t)Nmax, 1.0f);
        hpZ1=hpZ2=0; fullRms=hpRms=1e-6f;
        // Pre-size scratch buffers to avoid allocations on audio thread
        xScratch.resize((size_t)frameSz);
        nsdfScratch.resize((size_t)(maxTau + 2), 0.0f);
        reset();
    }
    void reset(){ w=0; have=false; f0Smooth=0.f; lastMidi=0.f; haveMidi=false; }

    // configuration
    void setKeyScale(int keyRoot_ /*0..11*/, uint16_t mask_) { keyRoot = keyRoot_; scaleMask = mask_; }
    void setRetune(float speed01_, int biasUpDown_){ retune01 = clamp01(speed01_); bias = biasUpDown_; }
    void setRange(float fMin_, float fMax_) {
        fMin = fMin_; fMax = fMax_;
        minTau = (int)std::floor(sr / fMax); maxTau = (int)std::ceil(sr / fMin);
        minTau = std::max(2, std::min(minTau, frameSz/2)); maxTau = std::max(minTau+2, std::min(maxTau, frameSz-2));
        // keep nsdf scratch sized
        nsdfScratch.resize((size_t)(maxTau + 2), 0.0f);
    }

    // run once per audio block (mono analysis input)
    PitchBlock analyze(const float* in, int N) {
        // 0) copy to sliding frame + make filtered copy for analysis
        for (int n=0; n<N; ++n){ ringPush(in[n]); }
        // f0 suggestion for bandpass (fallback mid if unknown)
        const float center = (f0Smooth>0.f ? f0Smooth : 200.f);
        setBandpass(center, (f0Smooth>0.f ? 2.5f : 0.8f));
        processBandpass(in, filt.data(), N);

        // 1) hop-based MPM/NSDF
        if (((hopCount += N) / hop) > 0){ hopCount %= hop; if (have) analyzeFrame(); }

        // 2) Targeting & per-sample ratio smoothing
        float fTarget = (f0Smooth>0.f) ? midiToHz( smoothToward(lastMidi, (float) nearestNoteInScale(hzToMidi(f0Smooth), keyRoot, scaleMask, bias),
                                                     (float)N / (float)sr, retuneTauSeconds(retune01)) ) : 0.f;

        // per-sample gentle one-pole toward ratioTarget
        const float ratioTarget = (f0Smooth>0.f && fTarget>0.f) ? (fTarget / f0Smooth) : 1.0f;
        const float a = 1.f - std::exp(-(float)N / (float)sr / 0.02f); // 20 ms feel
        float r = prevRatio;
        for (int n=0; n<N; ++n) { r = (1.f-a)*r + a*ratioTarget; ratioBuf[(size_t)n]=r; }
        prevRatio = r;

        // 3) sibilant flag (simple HP vs full RMS ratio, SR-normalized split)
        measureRms(in, N);
        const float hfRatio = hpRms / std::max(1e-6f, fullRms);
        PitchBlock out;
        out.N = N; out.ratio = ratioBuf.data();
        out.voiced = (f0Smooth>0.f);
        out.sibilant = (hfRatio > 0.35f); // tune per material
        out.f0 = f0Smooth;
        return out;
    }

private:
    // ----- UTIL: pitch units
    static inline float clamp01(float x){ return x<0.f?0.f:(x>1.f?1.f:x); }
    static inline float hzToMidi(float f){ return 69.f + 12.f * std::log2(std::max(1e-9f, f/440.f)); }
    static inline float midiToHz(float m){ return 440.f * std::pow(2.f, (m-69.f)/12.f); }

    static inline float retuneTauSeconds(float s){ // 0 slow .. 1 instant
        const float tMin=0.005f, tMax=0.35f; return tMax * std::pow(tMin/tMax, clamp01(s));
    }
    static inline float smoothToward(float cur, float target, float dt, float tau){
        if (tau<0.0007f) return target; float a = 1.f - std::exp(-dt/tau); return (1.f-a)*cur + a*target;
    }
    static inline bool inScale(int pcAbs, int keyRoot, uint16_t mask){
        int rel = (pcAbs - keyRoot)%12; if (rel<0) rel+=12; return (mask >> rel) & 1u;
    }
    static inline int nearestNoteInScale(float m, int keyRoot, uint16_t mask, int biasUpDown){
        int base = (int)std::floor(m), best=base; float bestD=1e9f;
        for (int k=-6;k<=6;++k){ int cand=base+k; int pc=(cand%12+12)%12; if(!inScale(pc,keyRoot,mask)) continue;
            float d=std::abs(m-(float)cand); if (std::abs(d-bestD)<1e-6f) d -= 1e-4f*(float)biasUpDown*((cand>=m)?1.f:-1.f);
            if (d<bestD){bestD=d;best=cand;} }
        return best;
    }

    // ----- band-pass (RBJ constant-skirt)
    void setBandpass(float f0, float Q){
        const double w0 = 2.0*M_PI*(double)std::clamp(f0, 10.f, 0.45f*(float)sr)/sr;
        const double cw = std::cos(w0), sw = std::sin(w0);
        const double alpha = sw/(2.0*std::max(0.1, (double)Q));
        const double a0=1.0+alpha, ia0=1.0/a0;
        b0=(float)(( Q*sw)*ia0); b1=0.f; b2=(float)((-Q*sw)*ia0);
        a1=(float)((-2.0*cw)*ia0); a2=(float)((1.0-alpha)*ia0);
    }
    inline float bpSample(float x){
        float y = b0*x + z1;
        z1 = b1*x - a1*y + z2;
        z2 = b2*x - a2*y;
        return y;
    }
    void processBandpass(const float* in, float* out, int N){
        for (int n=0;n<N;++n) out[n] = bpSample(in[n]);
    }

    // ----- RMS + simple HP for sibilant ratio
    void measureRms(const float* x, int N){
        // HP: 7 kHz at 48k, scale with SR
        const float fSplit = 7000.f * (float)(sr/48000.0);
        const float c = std::exp(-2.f*(float)M_PI*fSplit/(float)sr); // 1-pole HP via DC cancel
        float y=hpState;
        double sF=1e-12, sH=1e-12;
        for (int n=0;n<N;++n){ float xn=x[n]; y = c*y + c*(xn - hpPrev); hpPrev=xn; sF += (double)xn*xn; sH += (double)y*y; }
        hpState=y; fullRms = std::sqrt((float)(sF/N)); hpRms = std::sqrt((float)(sH/N));
    }

    // ----- ring / frame / NSDF (MPM-ish)
    void ringPush(float x){ frame[(size_t)w]=x; w=(w+1)%frameSz; have=true; }
    void unwrap(std::vector<float>& out) const{
        out.resize((size_t)frameSz);
        const int tail = frameSz - w;
        std::copy(frame.begin()+w, frame.end(), out.begin());
        std::copy(frame.begin(), frame.begin()+w, out.begin()+tail);
    }
    void analyzeFrame(){
        unwrap(xScratch);
        nsdfScratch.assign((size_t)(maxTau+2), 0.f);
        double e0=0.0; for (int i=0;i<frameSz;++i) e0 += (double)xScratch[(size_t)i]*xScratch[(size_t)i];
        for (int t=minTau;t<=maxTau;++t){
            const int L = frameSz - t; const float* x0=xScratch.data(); const float* xt=xScratch.data()+t;
            double ac=0.0, eT=0.0; int i=0;
            for (; i+3<L; i+=4){
                ac += (double)x0[i]*xt[i] + (double)x0[i+1]*xt[i+1] + (double)x0[i+2]*xt[i+2] + (double)x0[i+3]*xt[i+3];
                eT += (double)xt[i]*xt[i] + (double)xt[i+1]*xt[i+1] + (double)xt[i+2]*xt[i+2] + (double)xt[i+3]*xt[i+3];
            }
            for (; i<L; ++i){ ac += (double)x0[i]*xt[i]; eT += (double)xt[i]*xt[i]; }
            nsdfScratch[(size_t)t] = (float)((2.0*ac) / (e0 + eT + 1e-12));
        }
        // find peak after first neg/pos crossing
        int zc=minTau; while (zc<=maxTau && nsdfScratch[(size_t)zc]>0.f) ++zc; while (zc<=maxTau && nsdfScratch[(size_t)zc]<0.f) ++zc;
        int best=-1; float bestV=-1.f;
        for (int t=std::max(zc,minTau); t<=maxTau; ++t){
            if (nsdfScratch[(size_t)t]>0.f &&
                nsdfScratch[(size_t)t] > nsdfScratch[(size_t)std::max(t-1,minTau)] &&
                nsdfScratch[(size_t)t] >= nsdfScratch[(size_t)std::min(t+1,maxTau)]){
                if (nsdfScratch[(size_t)t] > bestV){ bestV = nsdfScratch[(size_t)t]; best = t; }
            }
        }
        bool voiced=false; float f0=0.f;
        if (best>0 && bestV>=0.6f){
            f0 = (float)(sr / (float)best); voiced=true;
        }
        if (voiced){
            if (f0Smooth<=0.f) f0Smooth=f0;
            const float a = 1.f - std::exp(-(float)hop/(float)sr / 0.03f); // 30 ms
            f0Smooth = (1.f-a)*f0Smooth + a*f0;
            // init MIDI smoother
            if (!haveMidi){ lastMidi = hzToMidi(f0Smooth); haveMidi=true; }
        } else {
            f0Smooth *= 0.98f; if (f0Smooth<1.f) f0Smooth=0.f;
        }
    }

    // ----- state
    double sr=48000.0; int Nmax=0;
    int frameSz=1024, hop=256, hopCount=0;
    int w=0; bool have=false;
    float fMin=70.f, fMax=800.f; int minTau=60, maxTau=640;
    std::vector<float> frame, filt, ratioBuf;
    float prevRatio=1.f, f0Smooth=0.f;
    int keyRoot = 0; uint16_t scaleMask = 0x0FFF; float retune01=0.6f; int bias=0;
    float lastMidi=0.f; bool haveMidi=false;

    // bandpass biquad
    float b0=0.f,b1=0.f,b2=0.f,a1=0.f,a2=0.f, z1=0.f,z2=0.f;

    // sibilant HP detector
    float hpState=0.f, hpPrev=0.f, fullRms=1e-6f, hpRms=1e-6f;

    // bandpass state fix (was duplicate variable names)
    float hpZ1=0.f, hpZ2=0.f;

    // scratch buffers (were static locals)
    std::vector<float> xScratch;
    std::vector<float> nsdfScratch;
};