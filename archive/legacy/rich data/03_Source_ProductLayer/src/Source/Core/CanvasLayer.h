/******************************************************************************
 * File: CanvasLayer.h
 * Description: Multi-layer canvas system for professional composition
 * 
 * Enables complex multi-track compositions with individual layer control,
 * blend modes, and per-layer audio routing for SpectralCanvas Pro.
 * 
 * Copyright (c) 2025 Spectral Audio Systems
 ******************************************************************************/

#pragma once
#include <JuceHeader.h>
#include <memory>
#include <vector>
#include <atomic>

/**
 * @brief Individual canvas layer with paint strokes and audio routing
 * 
 * Each layer maintains its own collection of paint strokes, visual properties,
 * and audio synthesis parameters. Layers can be independently controlled,
 * blended, and routed to different audio processors.
 */
class CanvasLayer
{
public:
    //==============================================================================
    // Layer Paint Data
    
    struct PaintStroke
    {
        juce::Path path;
        juce::Colour color;
        float intensity = 1.0f;
        std::vector<float> pressures;  // Pressure values along the stroke
        juce::Time timestamp;
        
        PaintStroke(juce::Colour c, float i) : color(c), intensity(i) 
        {
            pressures.reserve(100);
            timestamp = juce::Time::getCurrentTime();
        }
    };
    
    //==============================================================================
    // Blend Modes (Industry Standard)
    
    enum class BlendMode
    {
        Normal,         // Standard alpha blending
        Multiply,       // Darken by multiplication
        Screen,         // Lighten by inverse multiplication
        Overlay,        // Combination of multiply and screen
        SoftLight,      // Subtle overlay
        HardLight,      // Intense overlay
        ColorDodge,     // Brighten dramatically
        ColorBurn,      // Darken dramatically
        Add,            // Linear addition
        Subtract,       // Linear subtraction
        Difference,     // Absolute difference
        Exclusion       // Inverted difference
    };
    
    //==============================================================================
    // Constructor & Core Methods
    
    CanvasLayer(int layerIndex, const juce::String& name = "");
    ~CanvasLayer() = default;
    
    // Layer identification
    int getIndex() const { return index; }
    juce::String getName() const { return name; }
    void setName(const juce::String& newName) { name = newName; }
    
    // Visibility & opacity
    bool isVisible() const { return visible.load(); }
    void setVisible(bool shouldBeVisible) { visible.store(shouldBeVisible); }
    float getOpacity() const { return opacity.load(); }
    void setOpacity(float newOpacity) { opacity.store(juce::jlimit(0.0f, 1.0f, newOpacity)); }
    
    // Blend mode
    BlendMode getBlendMode() const { return blendMode.load(); }
    void setBlendMode(BlendMode mode) { blendMode.store(mode); }
    
    // Lock state (prevents editing)
    bool isLocked() const { return locked.load(); }
    void setLocked(bool shouldBeLocked) { locked.store(shouldBeLocked); }
    
    // Solo/mute for audio
    bool isSolo() const { return solo.load(); }
    void setSolo(bool shouldBeSolo) { solo.store(shouldBeSolo); }
    bool isMuted() const { return muted.load(); }
    void setMuted(bool shouldBeMuted) { muted.store(shouldBeMuted); }
    
    //==============================================================================
    // Paint Stroke Management
    
    void addPaintStroke(const PaintStroke& stroke);
    void clearStrokes();
    const std::vector<PaintStroke>& getStrokes() const { return paintStrokes; }
    int getStrokeCount() const { return static_cast<int>(paintStrokes.size()); }
    
    // Start a new stroke
    void beginStroke(juce::Point<float> position, juce::Colour color, float pressure);
    void continueStroke(juce::Point<float> position, float pressure);
    void endStroke();
    
    // Undo support
    void removeLastStroke();
    PaintStroke* getCurrentStroke();
    
    //==============================================================================
    // Rendering
    
    void render(juce::Graphics& g, juce::Rectangle<float> bounds) const;
    void renderWithBlendMode(juce::Graphics& g, juce::Rectangle<float> bounds, 
                             juce::Image& targetImage) const;
    
    //==============================================================================
    // Audio Routing
    
    struct AudioRoutingInfo
    {
        int outputChannel = 0;      // Which audio output channel
        float gain = 1.0f;          // Layer-specific gain
        float pan = 0.0f;           // -1.0 (left) to 1.0 (right)
        bool processEffects = true; // Apply effects to this layer
        int effectSlot = -1;        // Which effect slot to use (-1 = global)
    };
    
    AudioRoutingInfo& getAudioRouting() { return audioRouting; }
    const AudioRoutingInfo& getAudioRouting() const { return audioRouting; }
    
    //==============================================================================
    // Layer Metadata
    
    struct LayerStatistics
    {
        int strokeCount = 0;
        float averagePressure = 0.0f;
        juce::Colour dominantColor;
        juce::Rectangle<float> boundingBox;
        juce::Time lastModified;
    };
    
    LayerStatistics calculateStatistics() const;
    
    //==============================================================================
    // Serialization
    
    juce::ValueTree toValueTree() const;
    void fromValueTree(const juce::ValueTree& tree);
    
private:
    //==============================================================================
    // Core Properties
    
    int index;                                  // Layer index (0 = bottom)
    juce::String name;                         // User-defined layer name
    std::atomic<bool> visible{true};           // Layer visibility
    std::atomic<float> opacity{1.0f};          // Layer opacity (0-1)
    std::atomic<BlendMode> blendMode{BlendMode::Normal};
    std::atomic<bool> locked{false};           // Prevent editing when locked
    std::atomic<bool> solo{false};             // Solo for audio output
    std::atomic<bool> muted{false};            // Mute audio output
    
    //==============================================================================
    // Paint Data
    
    std::vector<PaintStroke> paintStrokes;
    std::unique_ptr<PaintStroke> currentStroke;  // Stroke being drawn
    mutable juce::CriticalSection strokeLock;    // Thread safety for strokes
    
    //==============================================================================
    // Audio Configuration
    
    AudioRoutingInfo audioRouting;
    
    //==============================================================================
    // Rendering Cache
    
    mutable juce::Image cachedImage;
    mutable bool cacheValid = false;
    
    void invalidateCache() const { cacheValid = false; }
    void updateCache(juce::Rectangle<float> bounds) const;
    
    //==============================================================================
    // Helper Methods
    
    void applyBlendMode(juce::Graphics& g, BlendMode mode, float opacity) const;
    juce::Colour blendColors(juce::Colour base, juce::Colour blend, BlendMode mode, float opacity) const;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CanvasLayer)
};

//==============================================================================
/**
 * @brief Manages multiple canvas layers for complex compositions
 * 
 * Provides layer stack management, rendering coordination, and audio routing
 * for multi-layer canvas compositions in SpectralCanvas Pro.
 */
class LayerManager
{
public:
    LayerManager();
    ~LayerManager() = default;
    
    // Layer stack management
    CanvasLayer* addLayer(const juce::String& name = "");
    void removeLayer(int index);
    void moveLayer(int fromIndex, int toIndex);
    void clearAllLayers();
    
    // Layer access
    CanvasLayer* getLayer(int index);
    const CanvasLayer* getLayer(int index) const;
    CanvasLayer* getActiveLayer() { return activeLayer; }
    void setActiveLayer(int index);
    
    int getLayerCount() const { return static_cast<int>(layers.size()); }
    
    // Rendering
    void renderAllLayers(juce::Graphics& g, juce::Rectangle<float> bounds) const;
    
    // Solo/mute management
    void updateSoloStates();
    bool hasAnySolo() const;
    
    // Serialization
    juce::ValueTree toValueTree() const;
    void fromValueTree(const juce::ValueTree& tree);
    
    // Maximum layers (for performance)
    static constexpr int MAX_LAYERS = 16;
    
private:
    std::vector<std::unique_ptr<CanvasLayer>> layers;
    CanvasLayer* activeLayer = nullptr;
    int nextLayerIndex = 0;
    
    mutable juce::CriticalSection layerLock;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LayerManager)
};