#pragma once

#include <span>
#include <cmath>

/**
 * Psychoacoustic Bands Precomputation - Phase 1
 * 
 * Builds frequency-domain matrices and thresholds for psychoacoustic masking.
 * Supports Bark24 scale (default) with ERB48 hooks for future extension.
 * 
 * All matrices use row-major storage for cache efficiency and SIMD optimization.
 */

namespace PsychoacousticMasking {

// Bark scale constants and utilities
struct BarkScale24 {
    static constexpr int NumBands = 24;
    static constexpr float MinFreqHz = 20.0f;
    static constexpr float MaxFreqHz = 20000.0f;
    
    // Bark scale conversion (Zwicker & Fastl)
    static float barkFromHz(float hz) noexcept;
    static float hzFromBark(float bark) noexcept;
    static float bandCenterHz(int band) noexcept;
    static float bandWidthHz(int band) noexcept;
};

// ERB scale (placeholder for Phase 2)
struct ERBScale48 {
    static constexpr int NumBands = 48;
    // Implementation TBD in Phase 2
};

// Spreading function constants (locked parameters)
struct SpreadingConstants {
    static constexpr float UpwardSlopeDzPerBark = -27.0f;    // dB per Bark (to higher f)
    static constexpr float DownwardSlopeDzPerBark = -10.0f;  // dB per Bark (to lower f)  
    static constexpr float FloorDb = -60.0f;                 // Minimum spreading value
    static constexpr float MaxRowSum = 1.0f;                 // Cap row sums (no energy expansion)
};

// ATH constants (ISO 226 approximation)
struct ATHConstants {
    static constexpr float FloorDb = -60.0f;                 // Minimum threshold
    static constexpr float ReferenceDb = 0.0f;               // Phase-1: no SPL calibration
};

} // namespace PsychoacousticMasking

class PsychoacousticBandsPrecompute {
public:
    
    // Matrix builders (thread-safe, can be called from any thread)
    static void buildPoolingMatrix(std::span<float> W_out, int K, int B, float sampleRate);
    static void buildSpreadingMatrix(std::span<float> S_out, int B);
    static void buildATHVector(std::span<float> ATH_out, int B, float sampleRate);
    
    // Validation utilities
    static bool validatePoolingMatrix(std::span<const float> W, int K, int B, float tolerance = 1e-4f);
    static bool validateSpreadingMatrix(std::span<const float> S, int B);
    static bool validateATHVector(std::span<const float> ATH, int B);
    
    // Scale selection (Phase 1: Bark24 only, ERB48 hooks)
    enum class ScaleType { Bark24, ERB48 };
    static int getBandCount(ScaleType scale);
    static float getBandCenterHz(ScaleType scale, int band, float sampleRate);

private:
    // Internal helpers
    static void buildBark24PoolingMatrix(std::span<float> W_out, int K, float sampleRate);
    static void buildBark24SpreadingMatrix(std::span<float> S_out);
    static void buildBark24ATHVector(std::span<float> ATH_out, float sampleRate);
    
    // Matrix utilities
    static void normalizeMatrixRows(std::span<float> matrix, int rows, int cols, float maxRowSum);
    static float triangularWindow(float freq, float center, float width) noexcept;
    static float iso226QuietThreshold(float hz) noexcept;
    
    // ERB placeholder (Phase 2)
    static void buildERB48PoolingMatrix(std::span<float> W_out, int K, float sampleRate);
    static void buildERB48SpreadingMatrix(std::span<float> S_out);
    static void buildERB48ATHVector(std::span<float> ATH_out, float sampleRate);
};