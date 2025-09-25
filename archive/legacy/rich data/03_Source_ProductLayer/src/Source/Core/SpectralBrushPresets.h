/******************************************************************************
 * File: SpectralBrushPresets.h
 * Description: Professional spectral brush preset system for SpectralCanvas Pro
 * 
 * Provides a comprehensive library of ready-made "spectral brushes" that combine
 * CDP-style effects with intuitive names and musical parameter settings.
 * Each brush is a complete spectral processing setup optimized for specific
 * musical contexts and creative workflows.
 * 
 * Innovation:
 * - Professional-grade preset system inspired by vintage EMU hardware
 * - Musically-tuned parameter combinations for instant creative results
 * - Intelligent brush morphing and blending capabilities
 * - Performance-optimized presets for real-time use
 * - Genre-specific brush collections
 * 
 * Copyright (c) 2025 Spectral Audio Systems
 ******************************************************************************/

#pragma once
#include <JuceHeader.h>
#include <vector>
#include <unordered_map>
#include <memory>
#include "CDPSpectralEngine.h"
#include "ColorToSpectralMapper.h"

/**
 * @brief Professional spectral brush preset system for creative workflows
 * 
 * Manages a comprehensive library of spectral processing presets that can be
 * applied instantly via paint colors, providing immediate access to complex
 * CDP-style effects with musically-tuned parameters.
 */
class SpectralBrushPresets
{
public:
    SpectralBrushPresets();
    ~SpectralBrushPresets() = default;
    
    //==============================================================================
    // Spectral Brush Definition
    
    struct SpectralBrush
    {
        // Basic identification
        juce::String name;                      // Human-readable name
        juce::String description;               // Detailed description
        juce::String category;                  // "Texture", "Rhythm", "Ambient", etc.
        juce::String genre;                     // Target musical genre
        
        // Visual representation
        juce::Colour associatedColor;           // Recommended paint color
        juce::Colour secondaryColor;            // For gradient/blend effects
        juce::String iconPath;                  // Path to brush icon
        
        // Spectral effect configuration
        CDPSpectralEngine::SpectralEffect primaryEffect;
        std::vector<std::pair<CDPSpectralEngine::SpectralEffect, float>> layeredEffects;
        
        // Parameter settings (effect-specific)
        std::unordered_map<std::string, float> parameters;
        
        // Color mapping override
        ColorToSpectralMapper::MappingMode recommendedMappingMode;
        bool useCustomColorMapping = false;
        
        // Performance characteristics
        float estimatedCPUUsage = 0.5f;        // 0-1, performance hint
        int recommendedFFTSize = 1024;          // Optimal FFT size
        float recommendedOverlap = 0.75f;       // Optimal overlap factor
        
        // Musical context
        float recommendedTempo = 120.0f;        // BPM recommendation
        bool tempoSyncRequired = false;         // Requires tempo synchronization
        float complexityLevel = 0.5f;           // 0 = simple, 1 = complex
        
        // Usage hints
        juce::String usageHint;                 // Brief usage instruction
        std::vector<juce::String> tags;         // Searchable tags
        
        // Preset metadata
        juce::String author;                    // Creator of this preset
        juce::String version;                   // Preset version
        juce::Time creationDate;               // When preset was created
        int useCount = 0;                       // Usage statistics
        float userRating = 0.0f;               // Average user rating (if applicable)
        
        SpectralBrush() = default;
        SpectralBrush(const juce::String& brushName, 
                     CDPSpectralEngine::SpectralEffect effect,
                     juce::Colour color);
    };
    
    //==============================================================================
    // Brush Categories & Collections
    
    enum class BrushCategory
    {
        Texture,            // For creating spectral textures and atmospheres
        Rhythm,             // For rhythmic and percussive effects  
        Ambient,            // For ambient and cinematic soundscapes
        Glitch,             // For glitch and digital artifacts
        Vintage,            // For retro and vintage sound processing
        Experimental,       // For experimental and avant-garde effects
        Electronic,         // For electronic music production
        Cinematic,          // For film and game audio
        Vocal,              // For vocal processing and manipulation
        Harmonic,           // For harmonic and tonal effects
        All                 // All categories
    };
    
    // Collection management
    void loadBrushCollection(BrushCategory category);
    void loadCustomBrushCollection(const juce::File& collectionFile);
    std::vector<SpectralBrush> getBrushesInCategory(BrushCategory category) const;
    std::vector<BrushCategory> getAvailableCategories() const;
    
    //==============================================================================
    // Brush Library Management
    
    // Add/remove brushes
    void addBrush(const SpectralBrush& brush);
    void removeBrush(const juce::String& brushName);
    void updateBrush(const juce::String& brushName, const SpectralBrush& updatedBrush);
    
    // Brush retrieval
    SpectralBrush* getBrush(const juce::String& brushName);
    const SpectralBrush* getBrush(const juce::String& brushName) const;
    std::vector<SpectralBrush> getAllBrushes() const;
    int getBrushCount() const;
    
    // Brush search and filtering
    std::vector<SpectralBrush> searchBrushes(const juce::String& searchTerm) const;
    std::vector<SpectralBrush> filterBrushesByTag(const juce::String& tag) const;
    std::vector<SpectralBrush> filterBrushesByGenre(const juce::String& genre) const;
    std::vector<SpectralBrush> filterBrushesByComplexity(float minComplexity, float maxComplexity) const;
    std::vector<SpectralBrush> getRecommendedBrushes(const juce::String& context) const;
    
    //==============================================================================
    // Brush Application & Integration
    
    // Apply brush to spectral engine
    void applyBrush(const juce::String& brushName, CDPSpectralEngine* spectralEngine);
    void applyBrushWithIntensity(const juce::String& brushName, 
                                CDPSpectralEngine* spectralEngine, 
                                float intensity);
    
    // Apply brush with color override
    void applyBrushWithColor(const juce::String& brushName,
                           CDPSpectralEngine* spectralEngine,
                           ColorToSpectralMapper* colorMapper,
                           juce::Colour paintColor);
    
    // Brush morphing and blending
    void morphBetweenBrushes(const juce::String& brushA, 
                           const juce::String& brushB,
                           float morphAmount,
                           CDPSpectralEngine* spectralEngine);
    
    void blendBrushes(const std::vector<juce::String>& brushNames,
                     const std::vector<float>& weights,
                     CDPSpectralEngine* spectralEngine);
    
    //==============================================================================
    // Smart Brush Selection
    
    struct BrushRecommendation
    {
        SpectralBrush brush;
        float relevanceScore = 0.0f;           // 0-1, how relevant this brush is
        juce::String reason;                   // Why this brush was recommended
        std::vector<juce::String> matchingTags; // Tags that matched the query
    };
    
    // Intelligent brush recommendations
    std::vector<BrushRecommendation> recommendBrushesForColor(juce::Colour color, int maxResults = 5);
    std::vector<BrushRecommendation> recommendBrushesForGenre(const juce::String& genre, int maxResults = 5);
    std::vector<BrushRecommendation> recommendBrushesForMood(const juce::String& mood, int maxResults = 5);
    std::vector<BrushRecommendation> recommendSimilarBrushes(const juce::String& referenceBrush, int maxResults = 5);
    
    //==============================================================================
    // Brush Creation & Customization
    
    // Create brush from current spectral engine state
    SpectralBrush createBrushFromCurrentState(const juce::String& name,
                                             const juce::String& description,
                                             CDPSpectralEngine* spectralEngine,
                                             ColorToSpectralMapper* colorMapper);
    
    // Customize existing brush
    SpectralBrush customizeBrush(const juce::String& baseBrushName,
                               const std::unordered_map<std::string, float>& parameterOverrides,
                               const juce::String& newName = {});
    
    // Generate brush variations
    std::vector<SpectralBrush> generateBrushVariations(const juce::String& baseBrushName, 
                                                      int numVariations = 5);
    
    //==============================================================================
    // Preset Management & Persistence
    
    // Save/load brush libraries
    bool saveBrushLibrary(const juce::File& file) const;
    bool loadBrushLibrary(const juce::File& file);
    
    // Import/export individual brushes
    bool exportBrush(const juce::String& brushName, const juce::File& file) const;
    bool importBrush(const juce::File& file);
    
    // User preset management
    void saveUserBrush(const SpectralBrush& brush);
    void removeUserBrush(const juce::String& brushName);
    std::vector<SpectralBrush> getUserBrushes() const;
    
    // Factory presets management
    void resetToFactoryPresets();
    void updateFactoryPresets();
    bool isFactoryPreset(const juce::String& brushName) const;
    
    //==============================================================================
    // Performance & Optimization
    
    struct PerformanceInfo
    {
        float estimatedLatency = 0.0f;         // Estimated processing latency (ms)
        float cpuUsageEstimate = 0.0f;         // Estimated CPU usage (0-1)
        int recommendedBufferSize = 512;        // Recommended audio buffer size
        bool requiresHighPrecision = false;    // Needs high-precision processing
        
        // Memory usage estimates
        size_t estimatedMemoryUsage = 0;       // Bytes
        bool requiresLargeBuffers = false;     // Needs large internal buffers
    };
    
    PerformanceInfo estimateBrushPerformance(const juce::String& brushName) const;
    std::vector<juce::String> getPerformanceOptimizedBrushes() const;
    std::vector<juce::String> getHighQualityBrushes() const;
    
    // Performance profiling
    void enablePerformanceProfiling(bool enable) { performanceProfilingEnabled = enable; }
    void recordBrushUsage(const juce::String& brushName, float processingTimeMs);
    
    //==============================================================================
    // User Interface Support
    
    struct BrushUIInfo
    {
        juce::Image thumbnail;                  // Brush thumbnail image
        juce::Path iconPath;                    // Vector icon
        juce::Colour primaryColor;              // Primary color for UI
        juce::Colour accentColor;               // Accent color
        
        // Interactive elements
        std::vector<juce::String> adjustableParameters; // User-adjustable parameters
        std::vector<std::pair<juce::String, juce::Range<float>>> parameterRanges;
        
        // Visual feedback
        bool hasVisualization = false;         // Supports real-time visualization
        bool hasColorResponse = true;          // Responds to paint color changes
        
        // Animation properties
        float animationSpeed = 1.0f;           // UI animation speed multiplier
        bool isAnimated = false;               // Has animated elements
    };
    
    BrushUIInfo getBrushUIInfo(const juce::String& brushName) const;
    void updateBrushUIInfo(const juce::String& brushName, const BrushUIInfo& uiInfo);
    
    // Generate thumbnails
    void generateBrushThumbnails(int thumbnailSize = 64);
    juce::Image generateBrushThumbnail(const juce::String& brushName, int size = 64);
    
    //==============================================================================
    // Events & Callbacks
    
    class Listener
    {
    public:
        virtual ~Listener() = default;
        
        virtual void brushLibraryChanged() {}
        virtual void brushAdded(const juce::String& brushName) {}
        virtual void brushRemoved(const juce::String& brushName) {}
        virtual void brushApplied(const juce::String& brushName) {}
        virtual void brushRecommendationsUpdated() {}
    };
    
    void addListener(Listener* listener);
    void removeListener(Listener* listener);
    
private:
    //==============================================================================
    // Internal Data Storage
    
    std::vector<SpectralBrush> brushLibrary;
    std::unordered_map<juce::String, size_t> brushNameToIndex;
    
    // Category organization
    std::unordered_map<BrushCategory, std::vector<juce::String>> categoryToBrushes;
    
    // User data
    std::vector<SpectralBrush> userBrushes;
    std::unordered_map<juce::String, int> brushUsageStats;
    
    // Performance data
    bool performanceProfilingEnabled = false;
    std::unordered_map<juce::String, std::vector<float>> brushPerformanceHistory;
    
    // UI data
    std::unordered_map<juce::String, BrushUIInfo> brushUIData;
    
    // Listeners
    juce::ListenerList<Listener> listeners;
    
    //==============================================================================
    // Factory Preset Definitions
    
    void initializeFactoryPresets();
    
    // Texture brushes
    SpectralBrush createSpectralSmearBrush();
    SpectralBrush createSpectralFogBrush();
    SpectralBrush createGranularCloudBrush();
    SpectralBrush createSpectralGlassBrush();
    
    // Rhythm brushes  
    SpectralBrush createArpeggiatorBrush();
    SpectralBrush createStutterBrush();
    SpectralBrush createRhythmicGateBrush();
    SpectralBrush createBeatSlicerBrush();
    
    // Ambient brushes
    SpectralBrush createSpectralPadBrush();
    SpectralBrush createEtherealWashBrush();
    SpectralBrush createDeepResonanceBrush();
    SpectralBrush createCosmicDriftBrush();
    
    // Glitch brushes
    SpectralBrush createDigitalCrushBrush();
    SpectralBrush createSpectralGlitchBrush();
    SpectralBrush createBitShuffleBrush();
    SpectralBrush createDataCorruptionBrush();
    
    // Vintage brushes
    SpectralBrush createVintageSpectralBlurBrush();
    SpectralBrush createAnalogWarmthBrush();
    SpectralBrush createTapeSpectralBrush();
    SpectralBrush createVinylSpectralBrush();
    
    // Electronic brushes
    SpectralBrush createSynthSpectralBrush();
    SpectralBrush createBassSynthBrush();
    SpectralBrush createLeadSynthBrush();
    SpectralBrush createPadSynthBrush();
    
    //==============================================================================
    // Utility Methods
    
    size_t findBrushIndex(const juce::String& brushName) const;
    bool brushExists(const juce::String& brushName) const;
    void updateBrushCategories();
    void validateBrush(SpectralBrush& brush);
    
    // Category management helpers
    juce::String getCategoryName(BrushCategory category) const;
    BrushCategory stringToBrushCategory(const juce::String& categoryName) const;
    
    // Brush similarity calculation
    float calculateBrushSimilarity(const SpectralBrush& brush1, const SpectralBrush& brush2) const;
    float calculateColorSimilarity(juce::Colour color1, juce::Colour color2) const;
    
    // Search utilities
    bool matchesSearchTerm(const SpectralBrush& brush, const juce::String& searchTerm) const;
    float calculateRelevanceScore(const SpectralBrush& brush, const juce::String& context) const;
    
    // Parameter interpolation for morphing
    std::unordered_map<std::string, float> interpolateParameters(
        const std::unordered_map<std::string, float>& paramsA,
        const std::unordered_map<std::string, float>& paramsB,
        float amount) const;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectralBrushPresets)
};