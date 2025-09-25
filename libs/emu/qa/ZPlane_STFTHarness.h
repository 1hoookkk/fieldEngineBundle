#pragma once
#include <vector>
#include <cmath>
#include <juce_dsp/juce_dsp.h>
#include "ZPlaneLogFreqLUT.h" // provides: lut.build(sr,N,bins,fLo,fHi,specBins), lut.binToIdx[]

namespace zplane::qa
{
    // Minimal STFT harness to compare time-domain cascade vs spectral template
    struct STFTHarness
    {
        juce::dsp::FFT fft { 10 }; // 1024-point
        LogFreqLUT lut;
        std::vector<float> window, acc;

        void prepare (double sr, int fftOrder = 10, int specBins = 256)
        {
            fft = juce::dsp::FFT(fftOrder);
            const int N = 1 << fftOrder;
            const int bins = N / 2 + 1;

            window.resize((size_t) N);
            for (int n = 0; n < N; ++n)
                window[(size_t) n] = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * n / (N - 1))); // Hann

            lut.build((float) sr, N, bins, 20.0f, 20000.0f, specBins);
            acc.assign((size_t) specBins, 0.0f);
        }

        // mono-in → log-magnitude (dB) on the LUT's log-frequency grid
        void analyze (const float* x, int N, std::vector<float>& outLogDB)
        {
            // JUCE's real-FFT operates on 2*N samples (real/imag interleaved).
            std::vector<float> td((size_t) (2 * N), 0.0f);
            for (int n = 0; n < N; ++n)
                td[(size_t) n] = x[n] * window[(size_t) n];

            // Directly compute magnitudes to avoid manual interleaving math.
            fft.performFrequencyOnlyForwardTransform(td.data(), true /* positive freqs only */);

            std::fill(acc.begin(), acc.end(), 0.0f);
            const int bins = N / 2 + 1;

            const float maxIdx = (float) (acc.size() - 1);

            for (int k = 1; k < bins; ++k) // skip DC
            {
                const float mag = td[(size_t) k] + 1e-12f;

                const float fiRaw = lut.binToIdx[(size_t) k]; // fractional log-index
                const float fi    = std::clamp(fiRaw, 0.0f, maxIdx);
                const int   i0    = std::clamp((int) std::floor(fi), 0, (int) acc.size() - 1);
                const int   i1    = std::min(i0 + 1, (int) acc.size() - 1);
                const float a     = fi - (float) i0;

                acc[(size_t) i0] += (1.0f - a) * mag;
                acc[(size_t) i1] += a * mag;
            }

            outLogDB.resize(acc.size());
            for (size_t i = 0; i < acc.size(); ++i)
                outLogDB[i] = 20.0f * std::log10(acc[i] + 1e-9f);
        }

        static float l2diff (const std::vector<float>& A, const std::vector<float>& B)
        {
            const size_t N = std::min(A.size(), B.size());
            double s = 0.0;
            for (size_t i = 0; i < N; ++i) { const double d = (double)A[i] - (double)B[i]; s += d * d; }
            return (float) std::sqrt(s / (double) N);
        }
    };
} // namespace zplane::qa
