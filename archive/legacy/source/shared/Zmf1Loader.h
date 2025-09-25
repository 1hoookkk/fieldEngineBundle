#pragma once
#include <array>
#include <vector>
#include <cstring>
#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>

/**
 * ZMF1 (Z-plane Model Format v1) Loader
 *
 * Runtime loader for pre-computed Z-plane morphing filter coefficients.
 * Designed for integration with JUCE plugins via Pamplejuce.
 *
 * Usage:
 *   Zmf1Loader loader;
 *   if (loader.loadFromPack(packData, packSize)) {
 *       loader.setMorphPosition(0.5f);
 *       auto coeffs = loader.getCoefficients(48000.0);
 *       // Apply coeffs to your biquad cascade
 *   }
 */
class Zmf1Loader
{
public:
    static constexpr size_t MAX_SECTIONS = 6;
    static constexpr size_t MAX_FRAMES = 32;

    struct BiquadCoeffs {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
        float a1 = 0.0f, a2 = 0.0f;
    };

    struct Header {
        char magic[4];      // 'ZMF1'
        uint16_t version;   // 0x0001
        uint16_t modelId;   // 0=vowel, 1=bell, 2=low
        uint8_t numFrames;  // Number of morph frames
        uint8_t numSections;// Biquad sections per frame
        uint32_t sampleRateRef; // Reference sample rate
        uint16_t reserved;
    };

    Zmf1Loader() = default;

    /**
     * Load ZMF1 data from a binary pack entry
     * @param data Pointer to ZMF1 binary data
     * @param size Size of data in bytes
     * @return true if loaded successfully
     */
    bool loadFromPack(const uint8_t* data, size_t size)
    {
        if (!data || size < sizeof(Header)) {
            DBG("Zmf1Loader: Invalid data or size");
            return false;
        }

        // Parse header
        std::memcpy(&header, data, sizeof(Header));

        // Validate magic
        if (std::memcmp(header.magic, "ZMF1", 4) != 0) {
            DBG("Zmf1Loader: Invalid magic");
            return false;
        }

        // Validate version
        if (header.version != 1) {
            DBG("Zmf1Loader: Unsupported version: " << header.version);
            return false;
        }

        // Validate dimensions
        if (header.numFrames == 0 || header.numFrames > MAX_FRAMES ||
            header.numSections == 0 || header.numSections > MAX_SECTIONS) {
            DBG("Zmf1Loader: Invalid dimensions");
            return false;
        }

        // Calculate expected size
        size_t expectedSize = sizeof(Header) +
                             header.numFrames * header.numSections * 5 * sizeof(float);

        if (size < expectedSize) {
            DBG("Zmf1Loader: Insufficient data");
            return false;
        }

        // Parse coefficient frames
        frames.clear();
        frames.resize(header.numFrames);

        const float* coeffPtr = reinterpret_cast<const float*>(data + sizeof(Header));

        for (int frameIdx = 0; frameIdx < header.numFrames; ++frameIdx) {
            frames[frameIdx].resize(header.numSections);

            for (int sectionIdx = 0; sectionIdx < header.numSections; ++sectionIdx) {
                auto& coeffs = frames[frameIdx][sectionIdx];
                coeffs.b0 = *coeffPtr++;
                coeffs.b1 = *coeffPtr++;
                coeffs.b2 = *coeffPtr++;
                coeffs.a1 = *coeffPtr++;
                coeffs.a2 = *coeffPtr++;
            }
        }

        isLoaded = true;
        currentMorphPos = 0.0f;

        DBG("Zmf1Loader: Loaded model " << header.modelId
            << " with " << (int)header.numFrames << " frames");

        return true;
    }

    /**
     * Set morph position for interpolation
     * @param position Morph position [0.0, 1.0]
     */
    void setMorphPosition(float position)
    {
        currentMorphPos = juce::jlimit(0.0f, 1.0f, position);
    }

    /**
     * Get interpolated coefficients for current morph position
     * @param targetSampleRate Target sample rate for coefficient adjustment
     * @return Array of biquad coefficients
     */
    std::array<BiquadCoeffs, MAX_SECTIONS> getCoefficients(double targetSampleRate) const
    {
        std::array<BiquadCoeffs, MAX_SECTIONS> result;

        if (!isLoaded || frames.empty()) {
            // Return neutral coefficients
            for (auto& coeffs : result) {
                coeffs.b0 = 1.0f;
                coeffs.b1 = 0.0f;
                coeffs.b2 = 0.0f;
                coeffs.a1 = 0.0f;
                coeffs.a2 = 0.0f;
            }
            return result;
        }

        // Calculate frame indices for interpolation
        float framePos = currentMorphPos * (header.numFrames - 1);
        int frameA = static_cast<int>(framePos);
        int frameB = std::min(frameA + 1, static_cast<int>(header.numFrames - 1));
        float t = framePos - frameA;

        // Sample rate compensation factor
        double srFactor = targetSampleRate / static_cast<double>(header.sampleRateRef);

        // Interpolate coefficients
        for (int i = 0; i < header.numSections; ++i) {
            const auto& coeffsA = frames[frameA][i];
            const auto& coeffsB = frames[frameB][i];

            auto& out = result[i];

            // Linear interpolation of coefficients
            out.b0 = coeffsA.b0 + t * (coeffsB.b0 - coeffsA.b0);
            out.b1 = coeffsA.b1 + t * (coeffsB.b1 - coeffsA.b1);
            out.b2 = coeffsA.b2 + t * (coeffsB.b2 - coeffsA.b2);
            out.a1 = coeffsA.a1 + t * (coeffsB.a1 - coeffsA.a1);
            out.a2 = coeffsA.a2 + t * (coeffsB.a2 - coeffsA.a2);

            // Apply sample rate compensation if needed
            if (std::abs(srFactor - 1.0) > 0.01) {
                // Simple frequency warping for sample rate conversion
                // This is a simplified approach - production would use bilinear transform
                float warpFactor = static_cast<float>(std::tan(juce::MathConstants<double>::pi * 0.25) /
                                                       std::tan(juce::MathConstants<double>::pi * 0.25 / srFactor));
                out.a1 *= warpFactor;
                // Note: Full bilinear transform would adjust all coefficients
            }
        }

        return result;
    }

    /**
     * Apply loaded coefficients directly to a JUCE IIR filter array
     */
    void applyToFilters(juce::dsp::IIR::Filter<float>* filters,
                       size_t numFilters,
                       double targetSampleRate) const
    {
        auto coeffs = getCoefficients(targetSampleRate);
        size_t numToApply = std::min(numFilters, static_cast<size_t>(header.numSections));

        for (size_t i = 0; i < numToApply; ++i) {
            const auto& c = coeffs[i];

            // Create JUCE IIR coefficients
            auto juceCoeffs = juce::dsp::IIR::ArrayCoefficients<float>::makeAllPass(targetSampleRate);

            // Override with our coefficients
            // JUCE uses: y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2] - a1*y[n-1] - a2*y[n-2]
            // We store: a1 and a2 with positive signs, so negate for JUCE
            juceCoeffs.coefficients[0] = c.b0;  // b0
            juceCoeffs.coefficients[1] = c.b1;  // b1
            juceCoeffs.coefficients[2] = c.b2;  // b2
            juceCoeffs.coefficients[3] = 1.0f;  // a0 (always 1)
            juceCoeffs.coefficients[4] = -c.a1; // a1 (negated)
            juceCoeffs.coefficients[5] = -c.a2; // a2 (negated)

            filters[i].coefficients = juceCoeffs;
        }
    }

    // Getters
    bool hasData() const { return isLoaded; }
    uint16_t getModelId() const { return header.modelId; }
    std::string getModelName() const
    {
        switch (header.modelId) {
            case 0: return "Vowel Morph";
            case 1: return "Bell/Metallic";
            case 2: return "Low/Formant";
            default: return "Unknown";
        }
    }

private:
    Header header{};
    std::vector<std::vector<BiquadCoeffs>> frames;
    float currentMorphPos = 0.0f;
    bool isLoaded = false;
};