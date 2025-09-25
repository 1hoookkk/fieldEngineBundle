#include "PsychoacousticBandsPrecompute.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <numeric>

using namespace PsychoacousticMasking;

// Bark scale functions (Zwicker & Fastl, exact formulas)
float BarkScale24::barkFromHz(float hz) noexcept {
    // Zwicker & Fastl: Bark = 13*atan(0.00076*f) + 3.5*atan((f/7500)^2)
    const float f_khz = hz * 0.001f;
    return 13.0f * std::atan(0.76f * f_khz) + 3.5f * std::atan((f_khz / 7.5f) * (f_khz / 7.5f));
}

float BarkScale24::hzFromBark(float bark) noexcept {
    // Approximate inverse (good enough for band centers)
    return 600.0f * std::sinh(bark / 4.0f);
}

float BarkScale24::bandCenterHz(int band) noexcept {
    assert(band >= 0 && band < NumBands);
    // Linear spacing in Bark domain: 0 to 24 Bark
    const float bark = (float(band) + 0.5f) * 24.0f / float(NumBands);
    return hzFromBark(bark);
}

float BarkScale24::bandWidthHz(int band) noexcept {
    assert(band >= 0 && band < NumBands);
    // Bark bandwidth ≈ 1 Bark = ~24/24 = 1.0 Bark per band
    const float bark = 1.0f;
    const float center_hz = bandCenterHz(band);
    const float center_bark = barkFromHz(center_hz);
    const float upper_hz = hzFromBark(center_bark + bark * 0.5f);
    const float lower_hz = hzFromBark(center_bark - bark * 0.5f);
    return upper_hz - lower_hz;
}

// Matrix builders
void PsychoacousticBandsPrecompute::buildPoolingMatrix(std::span<float> W_out, int K, int B, float sampleRate) {
    assert(W_out.size() == static_cast<size_t>(K * B));
    assert(B == BarkScale24::NumBands); // Phase 1: Bark24 only
    
    buildBark24PoolingMatrix(W_out, K, sampleRate);
    
    // Validate row sums ≈ 1.0
    assert(validatePoolingMatrix(W_out, K, B, 1e-3f));
}

void PsychoacousticBandsPrecompute::buildSpreadingMatrix(std::span<float> S_out, int B) {
    assert(S_out.size() == static_cast<size_t>(B * B));
    assert(B == BarkScale24::NumBands); // Phase 1: Bark24 only
    
    buildBark24SpreadingMatrix(S_out);
    
    // Validate spreading properties
    assert(validateSpreadingMatrix(S_out, B));
}

void PsychoacousticBandsPrecompute::buildATHVector(std::span<float> ATH_out, int B, float sampleRate) {
    assert(ATH_out.size() == static_cast<size_t>(B));
    assert(B == BarkScale24::NumBands); // Phase 1: Bark24 only
    
    buildBark24ATHVector(ATH_out, sampleRate);
    
    // Validate non-zero thresholds
    assert(validateATHVector(ATH_out, B));
}

// Bark24 pooling matrix: triangular weights with row sums = 1
void PsychoacousticBandsPrecompute::buildBark24PoolingMatrix(std::span<float> W_out, int K, float sampleRate) {
    const int B = BarkScale24::NumBands;
    const float nyquist = sampleRate * 0.5f;
    
    // Zero initialize
    std::fill(W_out.begin(), W_out.end(), 0.0f);
    
    // Build triangular pooling weights (row-major: W[k*B + b])
    for (int k = 0; k < K; ++k) {
        const float bin_hz = (float(k) * nyquist) / float(K - 1); // DC to Nyquist
        const float bin_bark = BarkScale24::barkFromHz(bin_hz);
        
        float* W_row = &W_out[k * B]; // Row k of W matrix
        
        // Find overlapping bands and compute triangular weights
        for (int b = 0; b < B; ++b) {
            const float band_center_hz = BarkScale24::bandCenterHz(b);
            const float band_width_hz = BarkScale24::bandWidthHz(b);
            const float weight = triangularWindow(bin_hz, band_center_hz, band_width_hz);
            W_row[b] = weight;
        }
        
        // Normalize row to sum = 1.0 (energy conservation)
        const float row_sum = std::accumulate(W_row, W_row + B, 0.0f);
        if (row_sum > 1e-6f) {
            const float scale = 1.0f / row_sum;
            for (int b = 0; b < B; ++b) {
                W_row[b] *= scale;
            }
        }
    }
}

// Bark24 spreading matrix: asymmetric slopes (-27/-10 dB per Bark)
void PsychoacousticBandsPrecompute::buildBark24SpreadingMatrix(std::span<float> S_out) {
    const int B = BarkScale24::NumBands;
    
    // Build spreading function S[b0*B + b] (row-major)
    for (int b0 = 0; b0 < B; ++b0) {
        float* S_row = &S_out[b0 * B]; // Row b0 of S matrix
        
        for (int b = 0; b < B; ++b) {
            const float delta_bark = float(b - b0); // Target - source
            
            // Asymmetric slopes (locked parameters)
            float dB_atten;
            if (delta_bark >= 0.0f) {
                // Upward masking (to higher frequencies): -27 dB/Bark
                dB_atten = SpreadingConstants::UpwardSlopeDzPerBark * delta_bark;
            } else {
                // Downward masking (to lower frequencies): -10 dB/Bark
                dB_atten = SpreadingConstants::DownwardSlopeDzPerBark * std::abs(delta_bark);
            }
            
            // Apply floor and convert to power
            dB_atten = std::max(dB_atten, SpreadingConstants::FloorDb);
            S_row[b] = std::pow(10.0f, dB_atten / 10.0f);
        }
        
        // Cap row sum to prevent energy expansion (locked: max 1.0)
        const float row_sum = std::accumulate(S_row, S_row + B, 0.0f);
        if (row_sum > SpreadingConstants::MaxRowSum) {
            const float scale = SpreadingConstants::MaxRowSum / row_sum;
            for (int b = 0; b < B; ++b) {
                S_row[b] *= scale;
            }
        }
    }
}

// Bark24 ATH vector: ISO 226 approximation
void PsychoacousticBandsPrecompute::buildBark24ATHVector(std::span<float> ATH_out, float sampleRate) {
    const int B = BarkScale24::NumBands;
    
    for (int b = 0; b < B; ++b) {
        const float center_hz = BarkScale24::bandCenterHz(b);
        const float ath_db = iso226QuietThreshold(center_hz);
        
        // Convert to power with floor
        const float ath_db_floored = std::max(ath_db, ATHConstants::FloorDb);
        ATH_out[b] = std::pow(10.0f, ath_db_floored / 10.0f);
    }
}

// Validation functions
bool PsychoacousticBandsPrecompute::validatePoolingMatrix(std::span<const float> W, int K, int B, float tolerance) {
    // Check row sums ≈ 1.0
    for (int k = 0; k < K; ++k) {
        const float* W_row = &W[k * B];
        const float row_sum = std::accumulate(W_row, W_row + B, 0.0f);
        if (std::abs(row_sum - 1.0f) > tolerance) {
            return false;
        }
    }
    return true;
}

bool PsychoacousticBandsPrecompute::validateSpreadingMatrix(std::span<const float> S, int B) {
    // Check asymmetry: S[0,1] should be much smaller than S[1,0] (due to slopes)
    const float upward = S[0 * B + 1];   // Low to high frequency
    const float downward = S[1 * B + 0]; // High to low frequency
    
    // Upward masking should be weaker than downward (-27 vs -10 dB/Bark)
    return upward < downward;
}

bool PsychoacousticBandsPrecompute::validateATHVector(std::span<const float> ATH, int B) {
    // Check all thresholds are positive and finite
    for (int b = 0; b < B; ++b) {
        if (ATH[b] <= 0.0f || !std::isfinite(ATH[b])) {
            return false;
        }
    }
    return true;
}

// Utility functions
float PsychoacousticBandsPrecompute::triangularWindow(float freq, float center, float width) noexcept {
    const float distance = std::abs(freq - center);
    const float half_width = width * 0.5f;
    if (distance >= half_width) return 0.0f;
    return 1.0f - (distance / half_width);
}

float PsychoacousticBandsPrecompute::iso226QuietThreshold(float hz) noexcept {
    // ISO 226 approximation (Terhardt)
    const float f_khz = hz * 0.001f;
    const float ath_db = 3.64f * std::pow(f_khz, -0.8f) 
                       - 6.5f * std::exp(-0.6f * (f_khz - 3.3f) * (f_khz - 3.3f)) 
                       + 0.001f * std::pow(f_khz, 4.0f);
    return ath_db;
}

// Scale utilities
int PsychoacousticBandsPrecompute::getBandCount(ScaleType scale) {
    switch (scale) {
        case ScaleType::Bark24: return BarkScale24::NumBands;
        case ScaleType::ERB48: return ERBScale48::NumBands;
        default: return BarkScale24::NumBands;
    }
}

float PsychoacousticBandsPrecompute::getBandCenterHz(ScaleType scale, int band, float sampleRate) {
    switch (scale) {
        case ScaleType::Bark24: return BarkScale24::bandCenterHz(band);
        case ScaleType::ERB48: 
            // ERB48 implementation TBD in Phase 2
            assert(false && "ERB48 not implemented in Phase 1");
            return 0.0f;
        default: return BarkScale24::bandCenterHz(band);
    }
}

// ERB placeholder functions (Phase 2)
void PsychoacousticBandsPrecompute::buildERB48PoolingMatrix(std::span<float> W_out, int K, float sampleRate) {
    // TODO: Implement in Phase 2
    (void)W_out; (void)K; (void)sampleRate;
    assert(false && "ERB48 not implemented in Phase 1");
}

void PsychoacousticBandsPrecompute::buildERB48SpreadingMatrix(std::span<float> S_out) {
    // TODO: Implement in Phase 2  
    (void)S_out;
    assert(false && "ERB48 not implemented in Phase 1");
}

void PsychoacousticBandsPrecompute::buildERB48ATHVector(std::span<float> ATH_out, float sampleRate) {
    // TODO: Implement in Phase 2
    (void)ATH_out; (void)sampleRate;
    assert(false && "ERB48 not implemented in Phase 1");
}