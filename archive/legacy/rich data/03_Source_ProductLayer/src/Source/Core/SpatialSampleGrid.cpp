/******************************************************************************
 * File: SpatialSampleGrid.cpp
 * Description: Implementation of spatial grid optimization for O(1) sample triggering
 * 
 * Maps paint canvas regions to sample slots for efficient triggering.
 * Integrates with PaintEngine's existing spatial grid for unified performance.
 * 
 * Copyright (c) 2025 Spectral Audio Systems
 ******************************************************************************/

#include "SpatialSampleGrid.h"
#include <algorithm>

//==============================================================================
// Static color definitions for visualization
//==============================================================================

const std::array<juce::Colour, SpatialSampleGrid::NUM_SAMPLE_SLOTS> SpatialSampleGrid::slotColors = 
{
    juce::Colour(0xFF6B5B),   // Slot 0: Warm Red
    juce::Colour(0x5B8CFF),   // Slot 1: Cool Blue
    juce::Colour(0x5BFF8C),   // Slot 2: Fresh Green
    juce::Colour(0xFFB85B),   // Slot 3: Orange
    juce::Colour(0xFF5B8C),   // Slot 4: Pink
    juce::Colour(0x8C5BFF),   // Slot 5: Purple
    juce::Colour(0x5BFFFF),   // Slot 6: Cyan
    juce::Colour(0xFFFF5B)    // Slot 7: Yellow
};

//==============================================================================
// Constructor & Initialization
//==============================================================================

SpatialSampleGrid::SpatialSampleGrid()
{
    // Initialize grid with no assignments
    clearAllMappings();
    
    // Setup default canvas dimensions
    initialize(1000.0f, 600.0f);
}

void SpatialSampleGrid::initialize(float width, float height)
{
    this->canvasWidth = width;
    this->canvasHeight = height;
    this->canvasRight = canvasWidth;
    this->canvasTop = canvasHeight;
    
    // Calculate cell dimensions
    cellWidth = canvasWidth / static_cast<float>(GRID_WIDTH);
    cellHeight = canvasHeight / static_cast<float>(GRID_HEIGHT);
    
    // Clear any existing mappings
    clearAllMappings();
}

void SpatialSampleGrid::setCanvasBounds(float left, float right, float bottom, float top)
{
    canvasLeft = left;
    canvasRight = right;
    canvasBottom = bottom;
    canvasTop = top;
    
    canvasWidth = right - left;
    canvasHeight = top - bottom;
    
    cellWidth = canvasWidth / static_cast<float>(GRID_WIDTH);
    cellHeight = canvasHeight / static_cast<float>(GRID_HEIGHT);
}

//==============================================================================
// Sample Slot Mapping
//==============================================================================

void SpatialSampleGrid::mapRegionToSampleSlot(int gridX, int gridY, int sampleSlot)
{
    if (gridX >= 0 && gridX < GRID_WIDTH && 
        gridY >= 0 && gridY < GRID_HEIGHT &&
        sampleSlot >= 0 && sampleSlot < NUM_SAMPLE_SLOTS)
    {
        grid[gridY][gridX].assignedSlot = sampleSlot;
        grid[gridY][gridX].hasGradient = false;
    }
}

void SpatialSampleGrid::mapRegionToSampleSlot(juce::Rectangle<int> gridRegion, int sampleSlot)
{
    // Clip region to grid bounds
    int x1 = juce::jmax(0, gridRegion.getX());
    int y1 = juce::jmax(0, gridRegion.getY());
    int x2 = juce::jmin(GRID_WIDTH, gridRegion.getRight());
    int y2 = juce::jmin(GRID_HEIGHT, gridRegion.getBottom());
    
    // Map all cells in region
    for (int y = y1; y < y2; ++y)
    {
        for (int x = x1; x < x2; ++x)
        {
            mapRegionToSampleSlot(x, y, sampleSlot);
        }
    }
}

void SpatialSampleGrid::mapVerticalGradient(int sampleSlot, float pitchRange)
{
    if (sampleSlot < 0 || sampleSlot >= NUM_SAMPLE_SLOTS) return;
    
    // Find all cells assigned to this sample slot
    for (int y = 0; y < GRID_HEIGHT; ++y)
    {
        for (int x = 0; x < GRID_WIDTH; ++x)
        {
            if (grid[y][x].assignedSlot == sampleSlot)
            {
                grid[y][x].hasGradient = true;
                grid[y][x].gradientStartValue = -pitchRange / 2.0f;
                grid[y][x].gradientEndValue = pitchRange / 2.0f;
                grid[y][x].gradientAngle = 90.0f; // Vertical
            }
        }
    }
}

void SpatialSampleGrid::mapHorizontalGradient(int sampleSlot, float panRange)
{
    if (sampleSlot < 0 || sampleSlot >= NUM_SAMPLE_SLOTS) return;
    
    // Find all cells assigned to this sample slot
    for (int y = 0; y < GRID_HEIGHT; ++y)
    {
        for (int x = 0; x < GRID_WIDTH; ++x)
        {
            if (grid[y][x].assignedSlot == sampleSlot)
            {
                grid[y][x].hasGradient = true;
                grid[y][x].gradientStartValue = 0.0f;
                grid[y][x].gradientEndValue = panRange;
                grid[y][x].gradientAngle = 0.0f; // Horizontal
            }
        }
    }
}

void SpatialSampleGrid::mapRadialGradient(int centerX, int centerY, int sampleSlot)
{
    if (sampleSlot < 0 || sampleSlot >= NUM_SAMPLE_SLOTS) return;
    
    // Calculate maximum radius
    float maxRadius = std::sqrt(static_cast<float>(GRID_WIDTH * GRID_WIDTH + GRID_HEIGHT * GRID_HEIGHT)) / 2.0f;
    
    for (int y = 0; y < GRID_HEIGHT; ++y)
    {
        for (int x = 0; x < GRID_WIDTH; ++x)
        {
            if (grid[y][x].assignedSlot == sampleSlot)
            {
                // Calculate distance from center
                float dx = static_cast<float>(x - centerX);
                float dy = static_cast<float>(y - centerY);
                float distance = std::sqrt(dx * dx + dy * dy);
                
                grid[y][x].hasGradient = true;
                grid[y][x].parameterGradient = distance / maxRadius;
                grid[y][x].gradientStartValue = 0.0f;
                grid[y][x].gradientEndValue = 1.0f;
            }
        }
    }
}

//==============================================================================
// Real-time Lookup (O(1) Performance)
//==============================================================================

SpatialSampleGrid::SampleTriggerInfo SpatialSampleGrid::getSampleTriggerInfo(float canvasX, float canvasY) const
{
    // Update performance metrics
    performanceMetrics.lookupCount.fetch_add(1);
    
    // Convert to grid coordinates
    juce::Point<int> gridPos = canvasToGrid(canvasX, canvasY);
    
    // Check cache
    if (gridPos == lastGridLookup)
    {
        performanceMetrics.cacheHits.fetch_add(1);
        return lastTriggerInfo;
    }
    
    // Create trigger info from grid cell
    SampleTriggerInfo info;
    
    if (gridPos.x >= 0 && gridPos.x < GRID_WIDTH && 
        gridPos.y >= 0 && gridPos.y < GRID_HEIGHT)
    {
        const GridCell& cell = grid[gridPos.y][gridPos.x];
        info = createTriggerInfo(cell, canvasX, canvasY);
    }
    
    // Update cache
    lastGridLookup = gridPos;
    lastTriggerInfo = info;
    
    return info;
}

SpatialSampleGrid::SampleTriggerInfo SpatialSampleGrid::getSampleTriggerInfoNormalized(float normX, float normY) const
{
    float canvasX = normX * canvasWidth + canvasLeft;
    float canvasY = normY * canvasHeight + canvasBottom;
    return getSampleTriggerInfo(canvasX, canvasY);
}

std::vector<SpatialSampleGrid::SampleTriggerInfo> SpatialSampleGrid::getSampleTriggerInfoBatch(
    const std::vector<juce::Point<float>>& points) const
{
    std::vector<SampleTriggerInfo> results;
    results.reserve(points.size());
    
    for (const auto& point : points)
    {
        results.push_back(getSampleTriggerInfo(point.x, point.y));
    }
    
    return results;
}

//==============================================================================
// Spatial Queries
//==============================================================================

std::vector<juce::Point<int>> SpatialSampleGrid::getCellsForSampleSlot(int sampleSlot) const
{
    std::vector<juce::Point<int>> cells;
    
    for (int y = 0; y < GRID_HEIGHT; ++y)
    {
        for (int x = 0; x < GRID_WIDTH; ++x)
        {
            if (grid[y][x].assignedSlot == sampleSlot)
            {
                cells.emplace_back(x, y);
            }
        }
    }
    
    return cells;
}

bool SpatialSampleGrid::hasAssignment(int gridX, int gridY) const
{
    if (gridX >= 0 && gridX < GRID_WIDTH && gridY >= 0 && gridY < GRID_HEIGHT)
    {
        return grid[gridY][gridX].assignedSlot >= 0;
    }
    return false;
}

bool SpatialSampleGrid::hasAssignment(juce::Rectangle<int> gridRegion) const
{
    int x1 = juce::jmax(0, gridRegion.getX());
    int y1 = juce::jmax(0, gridRegion.getY());
    int x2 = juce::jmin(GRID_WIDTH, gridRegion.getRight());
    int y2 = juce::jmin(GRID_HEIGHT, gridRegion.getBottom());
    
    for (int y = y1; y < y2; ++y)
    {
        for (int x = x1; x < x2; ++x)
        {
            if (grid[y][x].assignedSlot >= 0)
                return true;
        }
    }
    
    return false;
}

std::vector<SpatialSampleGrid::SampleTriggerInfo> SpatialSampleGrid::getNeighboringAssignments(int gridX, int gridY) const
{
    std::vector<SampleTriggerInfo> neighbors;
    
    // Check 8 neighboring cells
    for (int dy = -1; dy <= 1; ++dy)
    {
        for (int dx = -1; dx <= 1; ++dx)
        {
            if (dx == 0 && dy == 0) continue; // Skip center
            
            int nx = gridX + dx;
            int ny = gridY + dy;
            
            if (nx >= 0 && nx < GRID_WIDTH && ny >= 0 && ny < GRID_HEIGHT)
            {
                if (grid[ny][nx].assignedSlot >= 0)
                {
                    float canvasX = gridToCanvas(nx, ny).x;
                    float canvasY = gridToCanvas(nx, ny).y;
                    neighbors.push_back(createTriggerInfo(grid[ny][nx], canvasX, canvasY));
                }
            }
        }
    }
    
    return neighbors;
}

//==============================================================================
// Performance Optimization
//==============================================================================

void SpatialSampleGrid::resetPerformanceMetrics()
{
    performanceMetrics.lookupCount.store(0);
    performanceMetrics.cacheHits.store(0);
    performanceMetrics.averageLookupTime.store(0.0f);
}

//==============================================================================
// Visualization Support
//==============================================================================

juce::Rectangle<float> SpatialSampleGrid::getCellBounds(int gridX, int gridY) const
{
    if (gridX < 0 || gridX >= GRID_WIDTH || gridY < 0 || gridY >= GRID_HEIGHT)
        return {};
    
    float x = canvasLeft + gridX * cellWidth;
    float y = canvasBottom + gridY * cellHeight;
    
    return juce::Rectangle<float>(x, y, cellWidth, cellHeight);
}

juce::Rectangle<float> SpatialSampleGrid::getCellBoundsFromCanvas(float canvasX, float canvasY) const
{
    juce::Point<int> gridPos = canvasToGrid(canvasX, canvasY);
    return getCellBounds(gridPos.x, gridPos.y);
}

juce::Colour SpatialSampleGrid::getSampleSlotColor(int sampleSlot) const
{
    if (sampleSlot >= 0 && sampleSlot < NUM_SAMPLE_SLOTS)
        return slotColors[sampleSlot];
    
    return juce::Colours::grey;
}

//==============================================================================
// Configuration & Presets
//==============================================================================

void SpatialSampleGrid::clearAllMappings()
{
    for (int y = 0; y < GRID_HEIGHT; ++y)
    {
        for (int x = 0; x < GRID_WIDTH; ++x)
        {
            grid[y][x] = GridCell();
        }
    }
    
    // Clear cache
    lastGridLookup = juce::Point<int>(-1, -1);
    lastTriggerInfo = SampleTriggerInfo();
}

void SpatialSampleGrid::applyPresetMapping(int preset)
{
    clearAllMappings();
    
    switch (static_cast<PresetMapping>(preset))
    {
        case PresetMapping::LinearHorizontal:
        {
            // Divide canvas into 8 vertical strips
            int stripWidth = GRID_WIDTH / NUM_SAMPLE_SLOTS;
            for (int slot = 0; slot < NUM_SAMPLE_SLOTS; ++slot)
            {
                int x1 = slot * stripWidth;
                int x2 = (slot + 1) * stripWidth;
                mapRegionToSampleSlot(juce::Rectangle<int>(x1, 0, x2 - x1, GRID_HEIGHT), slot);
                mapVerticalGradient(slot, 24.0f); // 2 octave pitch range
            }
            break;
        }
        
        case PresetMapping::LinearVertical:
        {
            // Divide canvas into 8 horizontal strips
            int stripHeight = GRID_HEIGHT / NUM_SAMPLE_SLOTS;
            for (int slot = 0; slot < NUM_SAMPLE_SLOTS; ++slot)
            {
                int y1 = slot * stripHeight;
                int y2 = (slot + 1) * stripHeight;
                mapRegionToSampleSlot(juce::Rectangle<int>(0, y1, GRID_WIDTH, y2 - y1), slot);
                mapHorizontalGradient(slot, 1.0f); // Full pan range
            }
            break;
        }
        
        case PresetMapping::Grid2x4:
        {
            // 2 columns, 4 rows
            int colWidth = GRID_WIDTH / 2;
            int rowHeight = GRID_HEIGHT / 4;
            
            for (int slot = 0; slot < NUM_SAMPLE_SLOTS; ++slot)
            {
                int col = slot % 2;
                int row = slot / 2;
                
                int x1 = col * colWidth;
                int y1 = row * rowHeight;
                
                mapRegionToSampleSlot(juce::Rectangle<int>(x1, y1, colWidth, rowHeight), slot);
            }
            break;
        }
        
        case PresetMapping::Grid4x2:
        {
            // 4 columns, 2 rows
            int colWidth = GRID_WIDTH / 4;
            int rowHeight = GRID_HEIGHT / 2;
            
            for (int slot = 0; slot < NUM_SAMPLE_SLOTS; ++slot)
            {
                int col = slot % 4;
                int row = slot / 4;
                
                int x1 = col * colWidth;
                int y1 = row * rowHeight;
                
                mapRegionToSampleSlot(juce::Rectangle<int>(x1, y1, colWidth, rowHeight), slot);
            }
            break;
        }
        
        case PresetMapping::Radial:
        {
            // Center outward in rings
            int centerX = GRID_WIDTH / 2;
            int centerY = GRID_HEIGHT / 2;
            
            // Calculate ring radii
            float maxRadius = std::min(GRID_WIDTH, GRID_HEIGHT) / 2.0f;
            float ringWidth = maxRadius / 4.0f; // 4 rings for 8 slots (2 slots per ring)
            
            for (int y = 0; y < GRID_HEIGHT; ++y)
            {
                for (int x = 0; x < GRID_WIDTH; ++x)
                {
                    float dx = static_cast<float>(x - centerX);
                    float dy = static_cast<float>(y - centerY);
                    float distance = std::sqrt(dx * dx + dy * dy);
                    
                    // Determine ring
                    int ring = static_cast<int>(distance / ringWidth);
                    if (ring >= 4) ring = 3;
                    
                    // Determine quadrant for slot assignment
                    int quadrant = 0;
                    if (dx >= 0 && dy < 0) quadrant = 0;      // Top-right
                    else if (dx < 0 && dy < 0) quadrant = 1;  // Top-left
                    else if (dx < 0 && dy >= 0) quadrant = 2; // Bottom-left
                    else quadrant = 3;                         // Bottom-right
                    
                    // Alternate slots between quadrants in each ring
                    int slot = (ring * 2) + (quadrant % 2);
                    if (slot < NUM_SAMPLE_SLOTS)
                    {
                        grid[y][x].assignedSlot = slot;
                        mapRadialGradient(centerX, centerY, slot);
                    }
                }
            }
            break;
        }
        
        case PresetMapping::Corners:
        {
            // 4 corners + 4 edge centers
            int cornerSize = GRID_WIDTH / 4;
            
            // Corners
            mapRegionToSampleSlot(juce::Rectangle<int>(0, 0, cornerSize, cornerSize), 0); // Top-left
            mapRegionToSampleSlot(juce::Rectangle<int>(GRID_WIDTH - cornerSize, 0, cornerSize, cornerSize), 1); // Top-right
            mapRegionToSampleSlot(juce::Rectangle<int>(0, GRID_HEIGHT - cornerSize, cornerSize, cornerSize), 2); // Bottom-left
            mapRegionToSampleSlot(juce::Rectangle<int>(GRID_WIDTH - cornerSize, GRID_HEIGHT - cornerSize, cornerSize, cornerSize), 3); // Bottom-right
            
            // Edge centers
            int edgeWidth = GRID_WIDTH / 3;
            int edgeHeight = GRID_HEIGHT / 3;
            int centerX = GRID_WIDTH / 2 - edgeWidth / 2;
            int centerY = GRID_HEIGHT / 2 - edgeHeight / 2;
            
            mapRegionToSampleSlot(juce::Rectangle<int>(centerX, 0, edgeWidth, edgeHeight), 4); // Top center
            mapRegionToSampleSlot(juce::Rectangle<int>(GRID_WIDTH - edgeWidth, centerY, edgeWidth, edgeHeight), 5); // Right center
            mapRegionToSampleSlot(juce::Rectangle<int>(centerX, GRID_HEIGHT - edgeHeight, edgeWidth, edgeHeight), 6); // Bottom center
            mapRegionToSampleSlot(juce::Rectangle<int>(0, centerY, edgeWidth, edgeHeight), 7); // Left center
            break;
        }
        
        case PresetMapping::ChromaticKeyboard:
        {
            // Piano keyboard layout - white and black keys
            int whiteKeyWidth = GRID_WIDTH / 5; // 5 white keys visible
            int blackKeyWidth = whiteKeyWidth * 2 / 3;
            int blackKeyHeight = GRID_HEIGHT * 2 / 3;
            
            // White keys (C, D, E, F, G)
            int whiteSlots[] = {0, 2, 4, 5, 7};
            for (int i = 0; i < 5; ++i)
            {
                int x = i * whiteKeyWidth;
                mapRegionToSampleSlot(juce::Rectangle<int>(x, 0, whiteKeyWidth, GRID_HEIGHT), whiteSlots[i % 5]);
            }
            
            // Black keys (C#, D#, F#)
            int blackPositions[] = {whiteKeyWidth - blackKeyWidth/2, 
                                   2*whiteKeyWidth - blackKeyWidth/2,
                                   3*whiteKeyWidth - blackKeyWidth/2};
            int blackSlots[] = {1, 3, 6};
            
            for (int i = 0; i < 3; ++i)
            {
                if (blackPositions[i] + blackKeyWidth <= GRID_WIDTH)
                {
                    mapRegionToSampleSlot(juce::Rectangle<int>(blackPositions[i], 0, blackKeyWidth, blackKeyHeight), blackSlots[i]);
                }
            }
            break;
        }
        
        case PresetMapping::DrumPads:
        {
            // MPC-style 4x4 grid (using first 8 slots in 2x4 layout)
            int padWidth = GRID_WIDTH / 4;
            int padHeight = GRID_HEIGHT / 2;
            
            for (int slot = 0; slot < NUM_SAMPLE_SLOTS; ++slot)
            {
                int col = slot % 4;
                int row = slot / 4;
                
                int x = col * padWidth;
                int y = row * padHeight;
                
                mapRegionToSampleSlot(juce::Rectangle<int>(x, y, padWidth, padHeight), slot);
                
                // Add velocity sensitivity in center of each pad
                if (slot < NUM_SAMPLE_SLOTS)
                {
                    mapRadialGradient(x + padWidth/2, y + padHeight/2, slot);
                }
            }
            break;
        }
    }
}

//==============================================================================
// Helper Methods
//==============================================================================

juce::Point<int> SpatialSampleGrid::canvasToGrid(float canvasX, float canvasY) const
{
    int gridX = static_cast<int>((canvasX - canvasLeft) / cellWidth);
    int gridY = static_cast<int>((canvasY - canvasBottom) / cellHeight);
    
    // Clamp to grid bounds
    gridX = juce::jlimit(0, GRID_WIDTH - 1, gridX);
    gridY = juce::jlimit(0, GRID_HEIGHT - 1, gridY);
    
    return juce::Point<int>(gridX, gridY);
}

juce::Point<float> SpatialSampleGrid::gridToCanvas(int gridX, int gridY) const
{
    float canvasX = canvasLeft + (gridX + 0.5f) * cellWidth;
    float canvasY = canvasBottom + (gridY + 0.5f) * cellHeight;
    
    return juce::Point<float>(canvasX, canvasY);
}

float SpatialSampleGrid::calculateGradientValue(const GridCell& cell, float localX, float localY) const
{
    if (!cell.hasGradient)
        return cell.parameterGradient;
    
    float value = 0.0f;
    
    // Calculate gradient based on angle
    if (cell.gradientAngle == 0.0f) // Horizontal
    {
        value = localX;
    }
    else if (cell.gradientAngle == 90.0f) // Vertical
    {
        value = localY;
    }
    else // Angled gradient
    {
        float angleRad = cell.gradientAngle * juce::MathConstants<float>::pi / 180.0f;
        value = localX * std::cos(angleRad) + localY * std::sin(angleRad);
    }
    
    // Map to gradient range
    return cell.gradientStartValue + value * (cell.gradientEndValue - cell.gradientStartValue);
}

SpatialSampleGrid::SampleTriggerInfo SpatialSampleGrid::createTriggerInfo(const GridCell& cell, float canvasX, float canvasY) const
{
    SampleTriggerInfo info;
    
    info.sampleSlot = cell.assignedSlot;
    
    if (info.sampleSlot >= 0)
    {
        // Calculate local position within cell (0-1)
        juce::Point<int> gridPos = canvasToGrid(canvasX, canvasY);
        float localX = (canvasX - canvasLeft - gridPos.x * cellWidth) / cellWidth;
        float localY = (canvasY - canvasBottom - gridPos.y * cellHeight) / cellHeight;
        
        // Apply gradient mapping if enabled
        if (cell.hasGradient)
        {
            float gradientValue = calculateGradientValue(cell, localX, localY);
            
            // Map gradient to parameters based on gradient type
            if (cell.gradientAngle == 90.0f) // Vertical = pitch
            {
                info.pitchOffset = gradientValue;
            }
            else if (cell.gradientAngle == 0.0f) // Horizontal = pan
            {
                info.panPosition = gradientValue;
            }
            else // Radial or angled = velocity/filter
            {
                info.velocityScale = 0.5f + gradientValue * 0.5f;
                info.filterCutoff = gradientValue;
            }
        }
        else
        {
            // Default parameter mapping based on position
            info.pitchOffset = (localY - 0.5f) * 12.0f; // ±6 semitones
            info.panPosition = localX;
            info.velocityScale = 0.8f + localY * 0.2f;
        }
        
        // Additional parameters from position
        info.filterCutoff = 0.5f + localY * 0.5f;
        info.resonance = localX * 0.3f;
        info.distortion = juce::jlimit(0.0f, 1.0f, (1.0f - localY) * 0.2f);
    }
    
    return info;
}