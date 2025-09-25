#pragma once
#include <cstdint>
#include <type_traits>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4324) // structure was padded due to alignment specifier
#endif

/**
 * Spectral data frame for Audio Thread → UI Thread communication.
 * Contains FFT magnitude and phase data for visualization.
 */
struct SpectralFrame {
    static constexpr size_t FFT_SIZE = 512;
    static constexpr size_t NUM_BINS = FFT_SIZE / 2 + 1; // 257 bins for real FFT
    
    alignas(32) float magnitude[NUM_BINS];    // Spectral magnitudes
    alignas(32) float phase[NUM_BINS];        // Spectral phases
    
    uint32_t sequenceNumber = 0;              // Monotonic frame counter
    uint64_t timestamp = 0;                   // High-resolution timestamp
    uint64_t timestampSamples = 0;            // Sample-accurate timestamp
    float fundamentalFreq = 0.0f;             // Fundamental frequency
    float spectralCentroid = 0.0f;            // Spectral centroid
    
    SpectralFrame() {
        for (size_t i = 0; i < NUM_BINS; ++i) {
            magnitude[i] = 0.0f;
            phase[i] = 0.0f;
        }
    }
};

// Ensure RT safety
static_assert(std::is_trivially_copyable_v<SpectralFrame>, "SpectralFrame must be trivially copyable for RT safety");

#ifdef _MSC_VER
#pragma warning(pop)
#endif