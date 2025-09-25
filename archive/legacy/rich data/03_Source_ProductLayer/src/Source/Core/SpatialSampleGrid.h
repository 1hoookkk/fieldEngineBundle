/******************************************************************************
 * File: SpatialSampleGrid.h
 * Description: Spatial grid optimization for O(1) sample triggering
 * 
 * Maps paint canvas regions to sample slots for efficient triggering.
 * Integrates with PaintEngine's existing spatial grid for unified performance.
 * 
 * Copyright (c) 2025 Spectral Audio Systems
 ******************************************************************************/

#pragma once
#include <JuceHeader.h>
#include <vector>
#include <array>
#include <atomic>

// Forward declarations
class ForgeProcessor;
class SpectralSynthEngine;

/**
 * @brief Spatial grid for O(1) sample triggering based on paint position
 * 
 * Features:
 * - Divides canvas into grid cells for fast lookup
 * - Each cell maps to sample slots and parameters
 * - Integrates with PaintEngine spatial optimization
 * - Thread-safe for real-time audio
 */
class SpatialSampleGrid
{
public:
    SpatialSampleGrid();
    ~SpatialSampleGrid() = default;
    
    //==============================================================================
    // Grid Configuration
    
    static constexpr int GRID_WIDTH = 32;   // Match PaintEngine grid
    static constexpr int GRID_HEIGHT = 32;  // 32x32 grid cells
    static constexpr int NUM_SAMPLE_SLOTS = 8;
    
    // Initialize grid with canvas dimensions
    void initialize(float canvasWidth, float canvasHeight);
    void setCanvasBounds(float left, float right, float bottom, float top);
    
    //==============================================================================
    // Sample Slot Mapping
    
    struct SampleTriggerInfo
    {
        int sampleSlot = -1;              // Which sample slot (0-7)
        float pitchOffset = 0.0f;         // Pitch offset in semitones
        float velocityScale = 1.0f;       // Velocity scaling factor
        float panPosition = 0.5f;         // Pan position (0-1)
        
        // Advanced parameters from position
        float filterCutoff = 1.0f;        // Filter cutoff (0-1)
        float resonance = 0.0f;           // Filter resonance (0-1)
        float distortion = 0.0f;          // Distortion amount (0-1)
        
        bool isValid() const { return sampleSlot >= 0 && sampleSlot < NUM_SAMPLE_SLOTS; }
    };
    
    // Map grid regions to sample slots
    void mapRegionToSampleSlot(int gridX, int gridY, int sampleSlot);
    void mapRegionToSampleSlot(juce::Rectangle<int> gridRegion, int sampleSlot);
    
    // Advanced mapping with parameter gradients
    void mapVerticalGradient(int sampleSlot, float pitchRange = 24.0f);  // Y-axis pitch
    void mapHorizontalGradient(int sampleSlot, float panRange = 1.0f);   // X-axis pan
    void mapRadialGradient(int centerX, int centerY, int sampleSlot);    // Radial mapping
    
    //==============================================================================
    // Real-time Lookup (O(1) Performance)
    
    // Get sample trigger info from canvas position
    SampleTriggerInfo getSampleTriggerInfo(float canvasX, float canvasY) const;
    
    // Get sample trigger info from normalized position (0-1)
    SampleTriggerInfo getSampleTriggerInfoNormalized(float normX, float normY) const;
    
    // Batch lookup for paint strokes
    std::vector<SampleTriggerInfo> getSampleTriggerInfoBatch(
        const std::vector<juce::Point<float>>& points) const;
    
    //==============================================================================
    // Spatial Queries
    
    // Find all grid cells assigned to a sample slot
    std::vector<juce::Point<int>> getCellsForSampleSlot(int sampleSlot) const;
    
    // Check if a region has sample assignment
    bool hasAssignment(int gridX, int gridY) const;
    bool hasAssignment(juce::Rectangle<int> gridRegion) const;
    
    // Get neighboring cells with assignments
    std::vector<SampleTriggerInfo> getNeighboringAssignments(int gridX, int gridY) const;
    
    //==============================================================================
    // Performance Optimization
    
    struct PerformanceMetrics
    {
        std::atomic<int> lookupCount{0};
        std::atomic<int> cacheHits{0};
        std::atomic<float> averageLookupTime{0.0f};
        
        // Disable copy constructor/assignment due to atomics
        PerformanceMetrics() = default;
        PerformanceMetrics(const PerformanceMetrics&) = delete;
        PerformanceMetrics& operator=(const PerformanceMetrics&) = delete;
        
        float getCacheHitRate() const 
        { 
            int total = lookupCount.load();
            return total > 0 ? float(cacheHits.load()) / float(total) : 0.0f;
        }
    };
    
    const PerformanceMetrics& getPerformanceMetrics() const { return performanceMetrics; }
    void resetPerformanceMetrics();
    
    //==============================================================================
    // Visualization Support
    
    // Get grid cell bounds for drawing
    juce::Rectangle<float> getCellBounds(int gridX, int gridY) const;
    juce::Rectangle<float> getCellBoundsFromCanvas(float canvasX, float canvasY) const;
    
    // Get color for visualization
    juce::Colour getSampleSlotColor(int sampleSlot) const;
    
    //==============================================================================
    // Configuration & Presets
    
    // Clear all mappings
    void clearAllMappings();
    
    // Preset mappings for common layouts
    void applyPresetMapping(int preset);
    
    enum class PresetMapping
    {
        LinearHorizontal,    // Slots 0-7 left to right
        LinearVertical,      // Slots 0-7 bottom to top  
        Grid2x4,            // 2x4 grid layout
        Grid4x2,            // 4x2 grid layout
        Radial,             // Center outward
        Corners,            // 4 corners + 4 edges
        ChromaticKeyboard,  // Piano keyboard layout
        DrumPads            // MPC-style 4x4 pads
    };
    
private:
    //==============================================================================
    // Grid Data Structure
    
    struct GridCell
    {
        int assignedSlot = -1;           // Primary sample slot
        float parameterGradient = 0.0f;  // Parameter value (0-1)
        bool hasGradient = false;        // Uses gradient mapping
        
        // Gradient parameters
        float gradientStartValue = 0.0f;
        float gradientEndValue = 1.0f;
        float gradientAngle = 0.0f;     // For directional gradients
    };
    
    // Main grid storage (row-major order)
    std::array<std::array<GridCell, GRID_WIDTH>, GRID_HEIGHT> grid;
    
    //==============================================================================
    // Canvas Mapping
    
    float canvasWidth = 1000.0f;
    float canvasHeight = 600.0f;
    float canvasLeft = 0.0f;
    float canvasRight = 1000.0f;
    float canvasBottom = 0.0f;
    float canvasTop = 600.0f;
    
    float cellWidth = 0.0f;
    float cellHeight = 0.0f;
    
    //==============================================================================
    // Helper Methods
    
    // Convert canvas coordinates to grid indices
    juce::Point<int> canvasToGrid(float canvasX, float canvasY) const;
    juce::Point<float> gridToCanvas(int gridX, int gridY) const;
    
    // Gradient calculation
    float calculateGradientValue(const GridCell& cell, float localX, float localY) const;
    
    // Parameter mapping
    SampleTriggerInfo createTriggerInfo(const GridCell& cell, float canvasX, float canvasY) const;
    
    //==============================================================================
    // Performance Tracking
    
    mutable PerformanceMetrics performanceMetrics;
    
    // Cache for last lookup (simple optimization)
    mutable juce::Point<int> lastGridLookup{-1, -1};
    mutable SampleTriggerInfo lastTriggerInfo;
    
    //==============================================================================
    // Sample Slot Colors (for visualization)
    
    static const std::array<juce::Colour, NUM_SAMPLE_SLOTS> slotColors;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpatialSampleGrid)
};