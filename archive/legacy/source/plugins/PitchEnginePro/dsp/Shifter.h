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

    void prepare(double fs, Mode m, int rbPow2=15){ sr=fs; mode=m; psolaPrepare(); vrPrepare(rbPow2); }

    void processBlock(const float* in, float* out, int N, const float* ratio, float f0Hz = 0.0f){
        if (mode==TrackPSOLA) psolaProcess(in,out,N,ratio,f0Hz);
        else                  varRateHQ(in,out,N,ratio);
    }

    void setMode(Mode m){ mode=m; }

private:
    double sr=48000.0; Mode mode=TrackPSOLA;

    // ---- PSOLA (fast, low-latency)
    std::vector<float> rb; int rMask=0; int writePos=0; double synPhase=0.0; int Pcur=128;
    void psolaPrepare(){
        rb.assign(1<<14, 0.f); rMask=(int)rb.size()-1;
        // Warm the ring buffer for first grains
        const int warm = 512;
        writePos = warm & rMask;
        synPhase = 0.0;
        Pcur = 128;
    }
    static inline float hann(int i,int L){ return 0.5f-0.5f*std::cos(2.0*M_PI*(double)i/(double)(L-1)); }
    int findEpoch(int center, int radius){
        int best=center; int rsz=rMask+1; for (int off=-radius;off<=radius;++off){
            int i1=(center+off-1+rsz)%rsz, i2=(center+off+rsz)%rsz;
            float x1=rb[(size_t)i1], x2=rb[(size_t)i2];
            if (x1<=0.f && x2>0.f){ best=i2; break; }
        } return best;
    }
    void psolaProcess(const float* in, float* out, int N, const float* ratio, float f0Hz = 0.0f){
        // 0) Write input into ring buffer
        for (int n=0; n<N; ++n) {
            rb[(size_t)(writePos & rMask)] = in[n];
            ++writePos;
        }

        // Clear output buffer FIRST
        std::fill(out, out+N, 0.0f);

        // 1) Calculate periods with enhanced safety rails
        double rMean=0.0;
        int validRatios = 0;
        for (int n=0;n<N;++n) {
            if (std::isfinite(ratio[n]) && ratio[n] > 0.0f) {
                rMean += ratio[n];
                validRatios++;
            }
        }
        if (validRatios > 0) rMean /= validRatios;
        else rMean = 1.0; // Fallback if no valid ratios

        rMean = std::clamp(rMean, 0.25, 4.0); // Guard against extreme ratios

        const int Pdet = (f0Hz > 0.0f && std::isfinite(f0Hz) && f0Hz >= 50.0f && f0Hz <= 1000.0f) ?
            std::clamp((int)std::round(sr / f0Hz), 32, 512) :  // Actual period from f0
            std::clamp(Pcur, 32, 512);  // Fallback to previous estimate

        const int Ptar = std::clamp((int)std::round(Pdet / rMean), 24, 1024); // Target period with wider range

        Pcur = Pdet; // Update for next frame

        // 2) Place grains using persistent synthesis phase with safety bounds
        int placements = 0;
        const int half = std::clamp(Pdet / 2, 12, 256);  // Safe grain half-width
        double pos = synPhase;      // Start from previous block's phase

        // CRITICAL FIX: Ensure coverage from sample 0
        // Step back to ensure grains can cover the start of this block
        int stepBackCount = 0;
        while (pos > half && stepBackCount < 10) { // Prevent infinite loop
            pos -= Ptar;
            stepBackCount++;
        }

        // Ensure we start early enough to cover sample 0 with grain tails
        if (pos <= -half) pos = -half;

        int grainCount = 0;
        const int maxGrains = (N / 16) + 10; // Reasonable grain limit
        while (pos < N + half && grainCount < maxGrains) {
            const int centerOut = (int)std::floor(pos);

            // Skip grains that would be completely outside output range
            if (centerOut + half < 0 || centerOut - half >= N) {
                pos += Ptar;
                grainCount++;
                continue;
            }

            // Map output time to ring index aligned with current input
            const int centerIn = (writePos - (N - centerOut)) & rMask;

            // Find epoch near T0 (±Pdet/2) with bounds check
            const int searchRadius = std::min(half, (int)rb.size() / 8);
            const int epoch = findEpoch(centerIn, searchRadius);

            // OLA one-period Hann window with proper bounds checking
            const int L = 2*half + 1;
            for (int k=-half; k<=half; ++k) {
                const int rin = (epoch + k) & rMask;
                const int rout = centerOut + k;
                // Safe bounds check - avoid dangerous unsigned cast
                if (rout >= 0 && rout < N) {
                    out[rout] += rb[(size_t)rin] * hann(k+half, L);
                }
            }
            pos += Ptar;                    // Next synthesis mark
            ++placements;
            ++grainCount;
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

        // Debug output removed for RT-safety (printf violates audio thread constraints)

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