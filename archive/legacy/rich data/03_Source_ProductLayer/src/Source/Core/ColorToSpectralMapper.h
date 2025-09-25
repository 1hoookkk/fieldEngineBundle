#pragma once
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <vector>
#include <unordered_map>
#include <memory>

/**
 * Color-to-spectral parameter mapping system for SpectralCanvas Pro
 * 
 * Transforms paint colors into musical spectral processing parameters using
 * advanced HSB analysis and perceptually-tuned mapping curves.
 */
class ColorToSpectralMapper
{
public:
    ColorToSpectralMapper();
    ~ColorToSpectralMapper() = default;
    
    enum class MappingMode
    {
        HueToEffect,        // Hue controls effect type, saturation controls intensity
        SaturationDriven,   // Saturation primary, hue modifies parameters
        BrightnessDriven,   // Brightness primary, hue selects effect variation
        ProBeatmaker,       // Optimized for electronic music production
        Experimental,       // Maximum creative freedom and range
        Cinematic,          // Optimized for film/game audio
        Ambient             // Optimized for atmospheric/texture creation
    };
    
    struct ColorAnalysis
    {
        float hue = 0.0f;           // 0.0-1.0
        float saturation = 0.0f;    // 0.0-1.0  
        float brightness = 0.0f;    // 0.0-1.0
        float chroma = 0.0f;        // Color purity/vividness
        float colorTemperature = 0.0f; // Warm (0) to Cool (1)
        float colorEnergy = 0.0f;   // Perceptual energy/activity
        bool isPrimaryColor = false;
        juce::Colour originalColor;
        
        ColorAnalysis() = default;
        ColorAnalysis(juce::Colour color);
    };
    
    struct SpectralParameters
    {
        float frequency = 440.0f;       // Base frequency in Hz
        float amplitude = 0.0f;         // Amplitude 0.0-1.0
        float filterCutoff = 1.0f;      // Filter frequency 0.0-1.0
        float resonance = 0.0f;         // Filter resonance 0.0-1.0
        float spectralBlur = 0.0f;      // Spectral blur amount 0.0-1.0
        float spectralShift = 0.0f;     // Frequency shift 0.0-1.0
        float harmonicContent = 0.5f;   // Harmonic richness 0.0-1.0
        float pan = 0.5f;               // Stereo position 0.0-1.0
        
        float pressure = 1.0f;          // Paint pressure
        float velocity = 0.0f;          // Paint velocity
        float positionX = 0.5f;         // Canvas X position
        float positionY = 0.5f;         // Canvas Y position
    };
    
    struct MusicalScaling
    {
        float minValue = 0.0f;
        float maxValue = 1.0f;
        float defaultValue = 0.5f;
        
        enum class CurveType
        {
            Linear,
            Exponential,
            Logarithmic,
            S_Curve,
            Musical
        } curveType = CurveType::Linear;
        
        float curveFactor = 1.0f;
        bool invertMapping = false;
        
        float applyScaling(float normalizedValue) const;
    };
    
    // Core mapping functionality
    void setMappingMode(MappingMode mode);
    MappingMode getCurrentMappingMode() const { return currentMappingMode; }
    
    SpectralParameters mapColorToSpectralParameters(
        juce::Colour color,
        float pressure = 1.0f,
        float velocity = 0.0f,
        float positionX = 0.5f,
        float positionY = 0.5f
    );
    
    // Color analysis
    ColorAnalysis analyzeColor(juce::Colour color);
    float calculatePerceptualColorDistance(juce::Colour color1, juce::Colour color2);
    
    // Parameter scaling
    void setParameterScaling(const std::string& parameterName, const MusicalScaling& scaling);
    MusicalScaling getParameterScaling(const std::string& parameterName) const;
    
    // Real-time smoothing
    void updateColorProcessingState(juce::Colour newColor);
    void setColorSmoothingTime(float timeMs) { colorSmoothingTimeMs = timeMs; }
    
private:
    MappingMode currentMappingMode = MappingMode::HueToEffect;
    std::unordered_map<std::string, MusicalScaling> globalParameterScaling;
    
    // Color smoothing state
    juce::Colour smoothedColor = juce::Colours::black;
    juce::Colour previousColor = juce::Colours::black;
    float colorSmoothingTimeMs = 50.0f;
    float colorChangeThreshold = 0.05f;
    
    // Internal methods
    float calculateChroma(float hue, float saturation, float brightness);
    float calculateColorTemperature(float hue);
    float calculateColorEnergy(float saturation, float brightness);
    juce::Colour smoothColor(juce::Colour targetColor, juce::Colour currentColor, float smoothingFactor);
    void initializeGlobalParameterScaling();
    float calculateHueDistance(float hue1, float hue2);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ColorToSpectralMapper)
};