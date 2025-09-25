#pragma once
// FrequencyLUT.h
// RT-safe MIDI-to-frequency lookup table optimization for SpectralCanvas Pro
// Precomputes expensive log2/pow operations to avoid per-partial calculations
// Header-only, RT-safe, integrates with HarmonicQuantizer.h C Major scale
// 
// Usage:
//   FrequencyLUT::initialize();  // Call once during plugin initialization
//   float freq = FrequencyLUT::midiToFreq(69);  // A4 = 440Hz
//   FrequencyLUT::QuantizationResult result = FrequencyLUT::quantizeFrequency(freq, sigmaCents);

#include <array>
#include <cmath>
#include <algorithm>
#include <limits>
#include "HarmonicQuantizer.h"

namespace scp {

class FrequencyLUT
{
public:
    // MIDI note range [0..127] covers full MIDI spectrum
    static constexpr int kMidiMin = 0;
    static constexpr int kMidiMax = 127;
    static constexpr int kMidiRange = kMidiMax - kMidiMin + 1;
    
    // Frequency range for reverse lookups (approximate MIDI 0-127)
    static constexpr double kFreqMin = 8.1758;    // MIDI 0: C-1
    static constexpr double kFreqMax = 12543.85;  // MIDI 127: G9
    
    // Quantization cache parameters
    static constexpr int kQuantCacheSize = 256;   // Power of 2 for fast masking
    static constexpr int kQuantCacheMask = kQuantCacheSize - 1;
    
    // Cents interpolation resolution
    static constexpr double kCentsPerSemitone = 100.0;
    static constexpr double kMaxCentsOffset = 50.0;  // ±50 cents for interpolation

    struct QuantizationResult
    {
        float snappedFrequency = 0.0f;      // Final blended frequency
        float snapWeight = 0.0f;            // Weight [0..1] for amplitude boost
        int nearestMidi = 69;               // Nearest MIDI note in scale
        float centsOffset = 0.0f;           // Cents from original to snapped
    };

    // Initialize lookup tables - call once during plugin initialization
    static void initialize() noexcept
    {
        initializeMidiToFreqTable();
        initializeScaleTargetsTable();
        clearQuantizationCache();
        initialized_ = true;
    }

    // RT-safe MIDI to frequency lookup
    static inline float midiToFreq(int midiNote) noexcept
    {
        if (!initialized_) [[unlikely]] return 440.0f;
        
        // Clamp to valid MIDI range
        int note = std::clamp(midiNote, kMidiMin, kMidiMax);
        return midiToFreqTable_[note];
    }

    // RT-safe MIDI to frequency with fractional MIDI support
    static inline float midiToFreq(double midiFloat) noexcept
    {
        if (!initialized_) [[unlikely]] return 440.0f;
        
        // Clamp to valid range
        double clampedMidi = std::clamp(midiFloat, double(kMidiMin), double(kMidiMax));
        
        int baseMidi = int(clampedMidi);
        double frac = clampedMidi - double(baseMidi);
        
        // Handle edge case where baseMidi == kMidiMax
        if (baseMidi >= kMidiMax) return midiToFreqTable_[kMidiMax];
        
        // Linear interpolation between adjacent MIDI notes
        float baseFreq = midiToFreqTable_[baseMidi];
        float nextFreq = midiToFreqTable_[baseMidi + 1];
        
        return baseFreq + static_cast<float>(frac) * (nextFreq - baseFreq);
    }

    // RT-safe frequency to MIDI conversion (uses approximation for speed)
    static inline double freqToMidi(double frequency) noexcept
    {
        if (!initialized_ || frequency <= 0.0) [[unlikely]] return 69.0;
        
        // Clamp to reasonable frequency range
        double clampedFreq = std::clamp(frequency, kFreqMin, kFreqMax);
        
        // Fast approximation: freq = 440 * 2^((midi-69)/12)
        // Solving for midi: midi = 69 + 12 * log2(freq/440)
        return 69.0 + 12.0 * std::log2(clampedFreq / 440.0);
    }

    // RT-safe harmonic quantization with caching
    static inline QuantizationResult quantizeFrequency(double frequencyHz, double sigmaCents) noexcept
    {
        if (!initialized_) [[unlikely]]
        {
            QuantizationResult result;
            result.snappedFrequency = static_cast<float>(frequencyHz);
            result.snapWeight = 0.0f;
            result.nearestMidi = 69;
            result.centsOffset = 0.0f;
            return result;
        }

        // Check cache first (hash based on frequency and sigma)
        uint32_t cacheKey = hashFreqSigma(frequencyHz, sigmaCents) & kQuantCacheMask;
        const auto& cached = quantizationCache_[cacheKey];
        
        // Simple cache validation (not perfect but RT-safe)
        constexpr double kCacheFreqTolerance = 0.1;  // Hz
        constexpr double kCacheSigmaTolerance = 1.0; // cents
        if (std::abs(cached.inputFrequency - frequencyHz) < kCacheFreqTolerance &&
            std::abs(cached.inputSigma - sigmaCents) < kCacheSigmaTolerance)
        {
            return cached.result;
        }

        // Cache miss - compute and store
        QuantizationResult result = computeQuantization(frequencyHz, sigmaCents);
        
        // Update cache (RT-safe single write)
        auto& entry = quantizationCache_[cacheKey];
        entry.inputFrequency = frequencyHz;
        entry.inputSigma = sigmaCents;
        entry.result = result;
        
        return result;
    }

    // RT-safe nearest scale MIDI lookup
    static inline int getNearestScaleMidi(double frequencyHz) noexcept
    {
        if (!initialized_) [[unlikely]] return 69;
        
        double midiFloat = freqToMidi(frequencyHz);
        return findNearestScaleTarget(midiFloat);
    }

    // Check if LUT is initialized
    static inline bool isInitialized() noexcept { return initialized_; }

private:
    // Lookup tables
    static inline std::array<float, kMidiRange> midiToFreqTable_{};
    static inline std::array<int, kMidiRange> scaleTargets_{};     // Nearest C major MIDI for each MIDI note
    static inline std::array<float, kMidiRange> scaleDistances_{}; // Cents distance to nearest scale target
    
    // Quantization cache
    struct CacheEntry
    {
        double inputFrequency = 0.0;
        double inputSigma = 0.0;
        QuantizationResult result{};
    };
    static inline std::array<CacheEntry, kQuantCacheSize> quantizationCache_{};
    
    static inline bool initialized_ = false;

    // Initialize MIDI-to-frequency table
    static void initializeMidiToFreqTable() noexcept
    {
        for (int midi = kMidiMin; midi <= kMidiMax; ++midi)
        {
            // Standard MIDI tuning: A4 (MIDI 69) = 440 Hz
            // freq = 440 * 2^((midi - 69) / 12)
            double freq = 440.0 * std::pow(2.0, (double(midi) - 69.0) / 12.0);
            midiToFreqTable_[midi] = static_cast<float>(freq);
        }
    }

    // Initialize scale targets table for C Major quantization
    static void initializeScaleTargetsTable() noexcept
    {
        for (int midi = kMidiMin; midi <= kMidiMax; ++midi)
        {
            double midiFloat = double(midi);
            int nearestScale = findNearestScaleTarget(midiFloat);
            scaleTargets_[midi] = nearestScale;
            
            // Precompute cents distance
            double centsDist = (midiFloat - double(nearestScale)) * kCentsPerSemitone;
            scaleDistances_[midi] = static_cast<float>(std::abs(centsDist));
        }
    }

    // Find nearest C Major scale target for given MIDI note
    static int findNearestScaleTarget(double midiFloat) noexcept
    {
        int base = int(std::round(midiFloat));
        int bestMidi = base;
        int bestDist = std::numeric_limits<int>::max();
        
        // Search ±6 semitones (one tritone each way) - matches HarmonicQuantizer
        for (int k = -6; k <= 6; ++k)
        {
            int candidate = base + k;
            if (candidate < kMidiMin || candidate > kMidiMax) continue;
            
            int candidatePc = ((candidate % 12) + 12) % 12;
            
            // Check if candidate pitch class is in C Major scale
            bool inScale = false;
            for (int pc : C_MAJOR_PCS)
            {
                if (candidatePc == pc)
                {
                    inScale = true;
                    break;
                }
            }
            
            if (inScale)
            {
                int dist = std::abs(candidate - base);
                if (dist < bestDist)
                {
                    bestDist = dist;
                    bestMidi = candidate;
                }
            }
        }
        
        return bestMidi;
    }

    // Compute full quantization result (cache miss path)
    static QuantizationResult computeQuantization(double frequencyHz, double sigmaCents) noexcept
    {
        QuantizationResult result;
        
        if (frequencyHz <= 0.0)
        {
            result.snappedFrequency = 0.0f;
            result.snapWeight = 0.0f;
            result.nearestMidi = 69;
            result.centsOffset = 0.0f;
            return result;
        }
        
        // Convert to MIDI space
        double midiFloat = freqToMidi(frequencyHz);
        int nearestScaleMidi = findNearestScaleTarget(midiFloat);
        
        // Calculate cents offset
        double centsDist = (midiFloat - double(nearestScaleMidi)) * kCentsPerSemitone;
        
        // Clamp to reasonable range
        centsDist = std::clamp(centsDist, -600.0, 600.0);
        
        // Compute Gaussian snap weight
        double sigma = std::max(sigmaCents, 1e-12);  // Avoid division by zero
        double exponent = -(centsDist * centsDist) / (2.0 * sigma * sigma);
        exponent = std::max(exponent, -700.0);  // Prevent underflow
        double weight = std::exp(exponent);
        weight = std::clamp(weight, 0.0, 1.0);
        
        // Compute blended frequency
        double targetFreq = double(midiToFreqTable_[nearestScaleMidi]);
        double blendedFreq = (1.0 - weight) * frequencyHz + weight * targetFreq;
        
        // Fill result
        result.snappedFrequency = static_cast<float>(blendedFreq);
        result.snapWeight = static_cast<float>(weight);
        result.nearestMidi = nearestScaleMidi;
        result.centsOffset = static_cast<float>(centsDist);
        
        return result;
    }

    // Simple hash function for cache key generation
    static inline uint32_t hashFreqSigma(double freq, double sigma) noexcept
    {
        // Simple hash combining frequency and sigma
        uint32_t freqHash = static_cast<uint32_t>(freq * 1000.0); // 1mHz resolution
        uint32_t sigmaHash = static_cast<uint32_t>(sigma * 10.0);  // 0.1 cents resolution
        return freqHash ^ (sigmaHash << 16);
    }

    // Clear quantization cache
    static void clearQuantizationCache() noexcept
    {
        for (auto& entry : quantizationCache_)
        {
            entry.inputFrequency = 0.0;
            entry.inputSigma = 0.0;
            entry.result = QuantizationResult{};
        }
    }
};

} // namespace scp