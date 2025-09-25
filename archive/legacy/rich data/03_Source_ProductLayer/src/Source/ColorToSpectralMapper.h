/******************************************************************************
 * File: ColorToSpectralMapper.h
 * Description: Revolutionary color-to-spectral parameter mapping system
 * 
 * Transforms paint colors into musical spectral processing parameters using
 * advanced HSB analysis and perceptually-tuned mapping curves. This is the
 * bridge between visual art and sonic manipulation - the core innovation
 * that makes SpectralCanvas Pro's paint-to-sound workflow revolutionary.
 * 
 * Key Innovation:
 * - Perceptually-tuned HSB → spectral parameter mapping
 * - Multiple mapping modes for different musical contexts
 * - Real-time multi-color blending for simultaneous effects
 * - Intelligent parameter scaling for musical results
 * - Advanced color analysis including dominant color extraction
 * 
 * Copyright (c) 2025 Spectral Audio Systems
 ******************************************************************************/

#pragma once
#include <JuceHeader.h>
#include <vector>
#include <array>
#include <unordered_map>
#include <memory>
#include <chrono>
#include "CDPSpectralEngine.h"

/**
 * @brief Revolutionary color-to-spectral parameter mapping system
 * 
 * Provides sophisticated translation between paint colors and spectral 
 * processing parameters, enabling intuitive control of complex CDP-style
 * effects through natural painting gestures.
 */
class ColorToSpectralMapper
{
public:
    ColorToSpectralMapper();
    ~ColorToSpectralMapper() = default;
    
    //==============================================================================
    // Color Mapping Modes
    
    enum class MappingMode
    {
        // Basic mapping modes
        HueToEffect,        // Hue controls effect type, saturation controls intensity
        SaturationDriven,   // Saturation primary, hue modifies parameters
        BrightnessDriven,   // Brightness primary, hue selects effect variation
        
        // Advanced mapping modes  
        ChromaticScale,     // Maps colors to musical intervals/scales
        SpectralBands,      // Maps colors to frequency bands
        TemporalEffects,    // Maps colors to time-based effect parameters
        
        // Professional presets
        ProBeatmaker,       // Optimized for electronic music production
        Experimental,       // Maximum creative freedom and range
        Cinematic,          // Optimized for film/game audio
        Ambient,            // Optimized for atmospheric/texture creation
        
        Custom              // User-defined mapping
    };
    
    void setMappingMode(MappingMode mode);
    MappingMode getCurrentMappingMode() const { return currentMappingMode; }
    
    //==============================================================================
    // Core Color Analysis & Mapping
    
    struct ColorAnalysis
    {
        // Basic HSB components
        float hue = 0.0f;           // 0.0-1.0
        float saturation = 0.0f;    // 0.0-1.0  
        float brightness = 0.0f;    // 0.0-1.0
        
        // Advanced color properties
        float chroma = 0.0f;        // Color purity/vividness
        float colorTemperature = 0.0f; // Warm (0) to Cool (1)
        float colorEnergy = 0.0f;   // Perceptual energy/activity
        
        // Color harmony analysis
        bool isPrimaryColor = false;
        bool isComplementary = false;
        bool isAnalogous = false;
        
        // Original color for reference
        juce::Colour originalColor;
        
        ColorAnalysis() = default;  // Default constructor
        ColorAnalysis(juce::Colour color);
    };
    
    // Single color mapping
    CDPSpectralEngine::PaintSpectralData mapColorToSpectralData(
        juce::Colour color,
        float pressure = 1.0f,
        float velocity = 0.0f,
        float positionX = 0.5f,
        float positionY = 0.5f
    );
    
    // Multi-color blending for simultaneous effects
    std::vector<CDPSpectralEngine::PaintSpectralData> mapColorBlendToSpectralLayers(
        const std::vector<juce::Colour>& colors,
        const std::vector<float>& weights,
        float pressure = 1.0f,
        float velocity = 0.0f,
        float positionX = 0.5f,
        float positionY = 0.5f
    );
    
    //==============================================================================
    // Advanced Color Processing
    
    struct DominantColorAnalysis
    {
        std::vector<juce::Colour> dominantColors;
        std::vector<float> colorWeights;
        juce::Colour averageColor;
        float colorComplexity = 0.0f;      // 0 = monochrome, 1 = highly complex
        float colorContrast = 0.0f;        // Overall contrast measure
        
        // For paint stroke analysis
        juce::Colour strokeStartColor;
        juce::Colour strokeEndColor;
        bool hasColorGradient = false;
        float gradientDirection = 0.0f;    // Angle in radians
    };
    
    // Analyze complex color combinations (for paint strokes, gradients, etc.)
    DominantColorAnalysis analyzeDominantColors(const std::vector<juce::Colour>& colors);
    DominantColorAnalysis analyzeColorGradient(juce::Colour startColor, juce::Colour endColor);
    
    //==============================================================================
    // Musical Parameter Scaling
    
    struct MusicalScaling
    {
        // Parameter range definitions
        float minValue = 0.0f;
        float maxValue = 1.0f;
        float defaultValue = 0.5f;
        
        // Scaling curve types
        enum class CurveType
        {
            Linear,         // Direct linear mapping
            Exponential,    // Exponential curve (good for frequency/amplitude)
            Logarithmic,    // Logarithmic curve (good for time/musical intervals)
            S_Curve,        // S-shaped curve (smooth transitions)
            Quantized,      // Stepped/quantized values (good for discrete parameters)
            Musical         // Musically-tuned scaling (semitones, harmonic ratios)
        } curveType = CurveType::Linear;
        
        float curveFactor = 1.0f;           // Curve shaping parameter
        bool invertMapping = false;         // Invert the mapping direction
        
        // Apply scaling to a normalized value (0-1)
        float applyScaling(float normalizedValue) const;
    };
    
    // Define musical scaling for different parameters
    void setParameterScaling(const std::string& parameterName, const MusicalScaling& scaling);
    MusicalScaling getParameterScaling(const std::string& parameterName) const;
    
    //==============================================================================
    // Color-to-Effect Mapping Configuration
    
    struct EffectColorMapping
    {
        CDPSpectralEngine::SpectralEffect effect;
        float hueCenter = 0.0f;             // Center hue for this effect (0-1)
        float hueRange = 0.16f;             // Hue range (±range around center)
        float minIntensity = 0.0f;          // Minimum effect intensity
        float maxIntensity = 1.0f;          // Maximum effect intensity
        
        // Parameter mappings for this effect
        std::unordered_map<std::string, MusicalScaling> parameterMappings;
        
        // Visual feedback
        juce::Colour representativeColor;   // Color to show for this effect
        juce::String effectName;            // Human-readable name
        juce::String description;           // Description for UI tooltips
    };
    
    // Configure color-to-effect mappings
    void setEffectColorMapping(CDPSpectralEngine::SpectralEffect effect, const EffectColorMapping& mapping);
    EffectColorMapping getEffectColorMapping(CDPSpectralEngine::SpectralEffect effect) const;
    
    // Get all configured mappings
    std::vector<EffectColorMapping> getAllEffectMappings() const;
    
    //==============================================================================
    // Preset Mapping Systems
    
    struct MappingPreset
    {
        juce::String name;
        juce::String description;
        MappingMode mode;
        
        std::vector<EffectColorMapping> effectMappings;
        std::unordered_map<std::string, MusicalScaling> globalParameterScaling;
        
        // Preset-specific configuration
        float globalIntensityScale = 1.0f;
        float globalParameterSensitivity = 1.0f;
        bool enableColorBlending = true;
        bool enableAdvancedAnalysis = true;
        
        // Target use case information
        juce::String targetGenre;           // "Electronic", "Ambient", "Experimental", etc.
        float complexityLevel = 0.5f;       // 0 = beginner, 1 = advanced
        bool tempoSyncRecommended = true;
    };
    
    // Preset management
    void loadMappingPreset(const MappingPreset& preset);
    MappingPreset getCurrentMappingPreset() const;
    void saveMappingPreset(const juce::String& name, const juce::String& description);
    
    // Built-in presets
    void loadBuiltInPreset(MappingMode mode);
    std::vector<MappingPreset> getBuiltInPresets() const;
    
    //==============================================================================
    // Real-time Color Processing
    
    struct ColorProcessingState
    {
        // Color smoothing for stable parameters
        juce::Colour smoothedColor;
        ColorAnalysis smoothedAnalysis;
        
        // Change detection
        bool colorChanged = false;
        float colorChangeRate = 0.0f;       // Rate of color change
        juce::Colour previousColor;
        
        // Temporal analysis
        std::vector<juce::Colour> colorHistory;
        float averageHue = 0.0f;
        float averageSaturation = 0.0f;
        float averageBrightness = 0.0f;
        
        // Performance metrics
        std::chrono::high_resolution_clock::time_point lastUpdateTime;
        float processingTimeMs = 0.0f;
    };
    
    // Real-time color processing
    void updateColorProcessingState(juce::Colour newColor);
    const ColorProcessingState& getColorProcessingState() const { return processingState; }
    
    // Color smoothing configuration
    void setColorSmoothingTime(float timeMs) { colorSmoothingTimeMs = timeMs; }
    void setColorChangeThreshold(float threshold) { colorChangeThreshold = threshold; }
    void enableColorHistoryAnalysis(bool enable, int historySize = 32);
    
    //==============================================================================
    // Advanced Features
    
    // Color harmony detection and enhancement
    struct ColorHarmony
    {
        enum class HarmonyType
        {
            None,
            Monochromatic,
            Analogous,
            Complementary,
            Triadic,
            Tetradic,
            SplitComplementary
        } type = HarmonyType::None;
        
        std::vector<juce::Colour> harmonyColors;
        float harmonyStrength = 0.0f;       // How well colors match the harmony
        juce::String harmonyName;
    };
    
    ColorHarmony analyzeColorHarmony(const std::vector<juce::Colour>& colors);
    std::vector<juce::Colour> generateHarmoniousColors(juce::Colour baseColor, ColorHarmony::HarmonyType type);
    
    // Perceptual color distance calculation
    float calculatePerceptualColorDistance(juce::Colour color1, juce::Colour color2);
    
    // Color accessibility and visibility
    float calculateColorContrast(juce::Colour foreground, juce::Colour background);
    bool isColorAccessible(juce::Colour foreground, juce::Colour background);
    
    //==============================================================================
    // Integration & Workflow
    
    // Direct integration with CDPSpectralEngine
    void bindToSpectralEngine(CDPSpectralEngine* engine);
    void updateSpectralEngineFromColor(juce::Colour color, float pressure, float velocity);
    
    // Batch processing for paint strokes
    void processPaintStroke(const std::vector<juce::Colour>& strokeColors, 
                          const std::vector<float>& pressures,
                          const std::vector<juce::Point<float>>& positions);
    
    // Visualization support
    struct ColorVisualization
    {
        juce::Image colorWheel;                    // HSB color wheel
        juce::Image effectMap;                     // Color-to-effect mapping visualization
        std::vector<juce::Rectangle<float>> effectRegions; // Interactive regions
        
        // Real-time feedback
        juce::Colour currentColor;
        CDPSpectralEngine::SpectralEffect currentEffect;
        float currentIntensity = 0.0f;
        
        // Animation state
        float animationPhase = 0.0f;
        bool isAnimating = false;
    };
    
    ColorVisualization generateColorVisualization(int width, int height);
    void updateVisualizationAnimation(float deltaTime);
    
private:
    //==============================================================================
    // Internal State
    
    MappingMode currentMappingMode = MappingMode::HueToEffect;
    MappingPreset currentPreset;
    
    // Effect mappings storage
    std::unordered_map<CDPSpectralEngine::SpectralEffect, EffectColorMapping> effectMappings;
    
    // Global parameter scaling
    std::unordered_map<std::string, MusicalScaling> globalParameterScaling;
    
    // Real-time processing state
    ColorProcessingState processingState;
    float colorSmoothingTimeMs = 50.0f;
    float colorChangeThreshold = 0.05f;
    bool colorHistoryEnabled = true;
    int colorHistorySize = 32;
    
    // Connected spectral engine
    CDPSpectralEngine* connectedSpectralEngine = nullptr;
    
    //==============================================================================
    // Color Analysis Methods
    
    ColorAnalysis analyzeColor(juce::Colour color);
    float calculateChroma(float hue, float saturation, float brightness);
    float calculateColorTemperature(float hue);
    float calculateColorEnergy(float saturation, float brightness);
    
    // Color space conversions
    void rgbToHsb(juce::Colour rgb, float& hue, float& saturation, float& brightness);
    juce::Colour hsbToRgb(float hue, float saturation, float brightness);
    void rgbToLab(juce::Colour rgb, float& l, float& a, float& b);
    
    //==============================================================================
    // Mapping Implementation Methods
    
    CDPSpectralEngine::SpectralEffect hueToSpectralEffect(float hue, MappingMode mode);
    float mapSaturationToIntensity(float saturation, MappingMode mode);
    float mapBrightnessToParameter(float brightness, const std::string& parameterName);
    
    // Built-in mapping presets
    void initializeBuiltInPresets();
    MappingPreset createProBeatmakerPreset();
    MappingPreset createExperimentalPreset();
    MappingPreset createCinematicPreset();
    MappingPreset createAmbientPreset();
    
    // Color smoothing implementation
    juce::Colour smoothColor(juce::Colour targetColor, juce::Colour currentColor, float smoothingFactor);
    void updateColorHistory(juce::Colour newColor);
    
    // Parameter scaling initialization
    void initializeGlobalParameterScaling();
    
    //==============================================================================
    // Utility Methods
    
    float normalizeHue(float hue);              // Wrap hue to 0-1 range
    float calculateHueDistance(float hue1, float hue2);  // Shortest distance on hue circle
    bool isWithinHueRange(float hue, float center, float range);
    
    // Parameter validation and clamping
    float validateParameter(float value, const std::string& parameterName);
    CDPSpectralEngine::PaintSpectralData validateSpectralData(const CDPSpectralEngine::PaintSpectralData& data);
    
    //==============================================================================
    // Built-in Presets Storage
    
    std::vector<MappingPreset> builtInPresets;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ColorToSpectralMapper)
};