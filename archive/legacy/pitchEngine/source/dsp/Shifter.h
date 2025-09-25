// Shifter.h
#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct Shifter {
    enum Mode { TrackPSOLA, PrintHQ };

    void prepare(double fs, Mode m, int rbPow2=15);

    void processBlock(const float* in, float* out, int N, const float* ratio, float f0Hz = 0.0f, float confidence = 1.0f);

    void setMode(Mode m){ mode=m; }

private:
    double fs=48000.0; Mode mode=TrackPSOLA;

    // ---- PSOLA (fast, low-latency)
    float lastF0Hz = 0.0f;
    float holdF0Hz = 0.0f;
    int   holdSamplesLeft = 0;
    int   holdMaxSamples = 0; // set in prepare
    std::vector<float> rb; int rMask=0; int writePos=0; double synPhase=0.0; int Pcur=128;
    void psolaPrepare(){
        rb.assign(1<<14, 0.f); rMask=(int)rb.size()-1;
        // Warm the ring buffer for first grains
        const int warm = 512;
        writePos = warm & rMask;
        synPhase = 0.0;
        Pcur = 128;
        // when F0 drops, keep last period alive for ~80 ms before falling back
        holdMaxSamples = int(0.08 * fs);
        holdSamplesLeft = 0; holdF0Hz = 0.0f;
    }
    static inline float hann(int i,int L){ return 0.5f-0.5f*std::cos(2.0*M_PI*(double)i/(double)(L-1)); }
    int findEpoch(int center, int radius){
        int best=center; int rsz=rMask+1; for (int off=-radius;off<=radius;++off){
            int i1=(center+off-1+rsz)%rsz, i2=(center+off+rsz)%rsz;
            float x1=rb[(size_t)i1], x2=rb[(size_t)i2];
            if (x1<=0.f && x2>0.f){ best=i2; break; }
        } return best;
    }
    void psolaProcess(const float* in, float* out, int N, const float* ratio, float f0Hz = 0.0f, float confidence = 1.0f){
        // 0) Write input into ring buffer
        for (int n=0; n<N; ++n) {
            rb[(size_t)(writePos & rMask)] = in[n];
            ++writePos;
        }

        // Clear output buffer FIRST
        std::fill(out, out+N, 0.0f);

        // 1) Calculate periods
        double rMean=0.0; for (int n=0;n<N;++n) rMean+=ratio[n]; rMean/=std::max(1,N);
        rMean = std::isfinite(rMean) ? std::clamp(rMean, 0.25, 4.0) : 1.0f;

        const bool voiced = (f0Hz > 20.0f) && (confidence > 0.25f);
        
        if (voiced) {
            lastF0Hz = f0Hz;
            holdF0Hz = f0Hz;
            holdSamplesLeft = holdMaxSamples; // refresh hold on voiced
        } else {
            if (holdSamplesLeft > 0 && holdF0Hz > 20.0f) {
                // synthesize using last stable F0 (frozen) so you keep singing
                f0Hz = holdF0Hz;
                // schedule grains at 1/holdF0Hz with current ratio r0
                // ... reuse your PSOLA scheduling code with holdF0Hz ...
                holdSamplesLeft -= N;
            } else {
                // safety: passthrough dry audio (no pitch) so you never go silent
                std::memcpy(out, in, size_t(N) * sizeof(float));
                return;
            }
        }
        
        const int Pdet = (f0Hz > 0.0f) ?
            std::clamp((int)std::round(fs / f0Hz), 32, 512) :  // Actual period from f0
            std::clamp(Pcur, 32, 512);  // Fallback to previous estimate

        const int Ptar = std::clamp((int)std::round(Pdet / rMean), 32, 512); // Target period

        Pcur = Pdet; // Update for next frame

        // 2) Place grains using persistent synthesis phase
        int placements = 0;
        const int half = Pdet / 2;  // Define grain half-width early
        double pos = synPhase;      // Start from previous block's phase

        // CRITICAL FIX: Ensure coverage from sample 0
        // Step back to ensure grains can cover the start of this block
        while (pos > half) pos -= Ptar;

        // Ensure we start early enough to cover sample 0 with grain tails
        if (pos <= -half) pos = -half;

        while (pos < N + half) {           // Allow grain tails to extend past block
            const int centerOut = (int)std::floor(pos);

            // Skip grains that would be completely outside output range
            if (centerOut + half < 0 || centerOut - half >= N) {
                pos += Ptar;
                continue;
            }

            // Map output time to ring index aligned with current input
            const int centerIn = (writePos - (N - centerOut)) & rMask;

            // Find epoch near T0 (±Pdet/2)
            const int epoch = findEpoch(centerIn, half);

            // OLA one-period Hann window
            const int L = 2*half + 1;
            for (int k=-half; k<=half; ++k) {
                const int rin = (epoch + k) & rMask;
                const int rout = centerOut + k;
                if ((unsigned)rout < (unsigned)N)
                    out[rout] += rb[(size_t)rin] * hann(k+half, L);
            }
            pos += Ptar;                    // Next synthesis mark
            ++placements;
        }

        // 3) Carry residual phase into next block
        synPhase = pos - N;                 // Keep [0..Ptar) for continuity

        // Calculate peak and check for NaNs
        float peak=1e-6f;
        int nanCount = 0;
        for (int n=0; n<N; ++n) {
            if (!std::isfinite(out[n])) { out[n] = 0.0f; ++nanCount; }
            peak = std::max(peak, std::abs(out[n]));
        }

        // Debug diagnostics removed for real-time safety

        // Apply normalization
        const float g = (peak>1.f)?(1.f/peak):1.f;
        for (int n=0;n<N;++n) out[n]*=g;
    }

    // ---- HQ variable-rate resampler (Print mode)
    std::vector<float> vrb; int vMask=0; int vw=0; double rpos=64.0;
    void vrPrepare(int pow2){ vrb.assign(1<<pow2, 0.f); vMask=(int)vrb.size()-1; vw=0; rpos=64.0; }
    static inline float lag4(float x0,float x1,float x2,float x3,float t){
        float a=(-1.f/6.f)*x0+0.5f*x1-0.5f*x2+(1.f/6.f)*x3;
        float b=0.5f*x0-x1+0.5f*x2;
        float c=(-1.f/3.f)*x0-0.5f*x1+x2-(1.f/6.f)*x3;
        float d=x1; return ((a*t+b)*t+c)*t+d;
    }
    void varRateHQ(const float* in, float* out, int N, const float* ratio){
        const int L=96; for (int n=0;n<N;++n){
            vrb[(size_t)(vw & vMask)] = in[n];
            int i=(int)rpos; float t=(float)(rpos-i);
            float x0=vrb[(i-1)&vMask], x1=vrb[i&vMask], x2=vrb[(i+1)&vMask], x3=vrb[(i+2)&vMask];
            out[n]=lag4(x0,x1,x2,x3,t);
            ++vw;
            rpos += ratio[n];
            const double minR = (double)(vw - L - 2);
            if (rpos > minR) rpos = minR;
            if (rpos < 2.0)  rpos = 2.0;
        }
        // optional: add 2-pole LPF when average ratio < 1.0 (downshift) to suppress imaging
    }
};