#pragma once

#include <JuceHeader.h>
#include <complex>
#include <array>
#include <vector>

namespace FieldEngineFX {

/**
 * Z-plane filter state representation for EMU-style filters
 */
struct ZPlaneState {
    // Pole and zero arrays (max 16 each for EMU compatibility)
    std::vector<std::complex<float>> poles;
    std::vector<std::complex<float>> zeros;
    
    // Filter characteristics
    float cutoff = 1000.0f;      // Hz
    float resonance = 0.0f;      // 0-1
    float gain = 1.0f;           // Linear gain
    
    // EMU-specific parameters
    int filterType = 0;          // 0-127 EMU filter types
    float morphPosition = 0.0f;  // 0-1 morph between filter types
    
    // Computed values
    float magnitude = 1.0f;
    float phase = 0.0f;
    
    // Helper methods
    void normalizeCoefficients();
    std::complex<float> evaluateTransferFunction(float frequency, float sampleRate) const;
    void computeFrequencyResponse(float frequency, float sampleRate);
};

/**
 * EMU filter bank with morphing capability
 */
class EMUFilterBank {
public:
    static constexpr int NUM_FILTER_TYPES = 128;
    
    EMUFilterBank();
    
    // Get filter state for a specific type
    ZPlaneState getFilterState(int filterType) const;
    
    // Morph between two filter types
    ZPlaneState morphFilters(int typeA, int typeB, float morphAmount) const;
    
    // Get filter name/description
    juce::String getFilterName(int filterType) const;
    juce::String getFilterDescription(int filterType) const;
    
private:
    struct FilterDefinition {
        juce::String name;
        juce::String description;
        std::vector<std::complex<float>> poles;
        std::vector<std::complex<float>> zeros;
        float defaultCutoff;
        float defaultResonance;
    };
    
    std::array<FilterDefinition, NUM_FILTER_TYPES> filterDefinitions;
    
    void initializeFilterDefinitions();
    void loadEMUFilterData();
};

/**
 * Real-time safe filter state interpolator
 */
class FilterStateInterpolator {
public:
    FilterStateInterpolator();
    
    void setTargetState(const ZPlaneState& target);
    void setSmoothingTime(float seconds) { smoothingTime = seconds; }
    
    ZPlaneState getCurrentState() const;
    void process(int numSamples, float sampleRate);
    
    bool isSmoothing() const { return currentTime < smoothingTime; }
    
private:
    ZPlaneState currentState;
    ZPlaneState targetState;
    ZPlaneState startState;
    
    float smoothingTime = 0.02f; // 20ms default
    float currentTime = 0.0f;
    
    ZPlaneState interpolate(const ZPlaneState& a, const ZPlaneState& b, float t) const;
};

} // namespace FieldEngineFX
