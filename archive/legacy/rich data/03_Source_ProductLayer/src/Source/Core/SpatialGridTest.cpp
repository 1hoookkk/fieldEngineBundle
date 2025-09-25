/******************************************************************************
 * File: SpatialGridTest.cpp
 * Description: Test utility for spatial grid sample triggering
 * 
 * Verifies that the O(1) spatial grid lookup and sample triggering
 * integration works correctly with various presets and paint coordinates.
 * 
 * Copyright (c) 2025 Spectral Audio Systems
 ******************************************************************************/

#include "SpatialSampleGrid.h"
#include <iostream>
#include <cassert>

/**
 * @brief Simple test utility for spatial grid functionality
 */
class SpatialGridTest
{
public:
    static void runAllTests()
    {
        std::cout << "🎯 Running Spatial Grid Tests...\n\n";
        
        testBasicGridLookup();
        testPresetMappings();
        testGradientMappings();
        testPerformanceMetrics();
        
        std::cout << "✅ All Spatial Grid Tests Passed!\n";
    }
    
private:
    static void testBasicGridLookup()
    {
        std::cout << "Testing basic grid lookup...\n";
        
        SpatialSampleGrid grid;
        grid.initialize(1000.0f, 600.0f);
        
        // Test simple mapping
        grid.mapRegionToSampleSlot(0, 0, 0); // Top-left to sample 0
        grid.mapRegionToSampleSlot(31, 31, 7); // Bottom-right to sample 7
        
        // Test lookup
        auto info1 = grid.getSampleTriggerInfo(0, 0);
        assert(info1.sampleSlot == 0);
        assert(info1.isValid());
        
        auto info2 = grid.getSampleTriggerInfo(999, 599);
        assert(info2.sampleSlot == 7);
        assert(info2.isValid());
        
        // Test empty region
        auto info3 = grid.getSampleTriggerInfo(500, 300);
        assert(!info3.isValid()); // Should be -1 for unmapped region
        
        std::cout << "✓ Basic grid lookup working\n";
    }
    
    static void testPresetMappings()
    {
        std::cout << "Testing preset mappings...\n";
        
        SpatialSampleGrid grid;
        grid.initialize(1000.0f, 600.0f);
        
        // Test Linear Horizontal preset
        grid.applyPresetMapping(static_cast<int>(SpatialSampleGrid::PresetMapping::LinearHorizontal));
        
        // Check that left side maps to sample 0
        auto infoLeft = grid.getSampleTriggerInfo(50, 300);
        assert(infoLeft.sampleSlot == 0);
        
        // Check that right side maps to sample 7
        auto infoRight = grid.getSampleTriggerInfo(950, 300);
        assert(infoRight.sampleSlot == 7);
        
        // Test Grid 2x4 preset
        grid.applyPresetMapping(static_cast<int>(SpatialSampleGrid::PresetMapping::Grid2x4));
        
        // Should have different mapping now
        auto infoGrid = grid.getSampleTriggerInfo(50, 50);
        assert(infoGrid.isValid());
        
        std::cout << "✓ Preset mappings working\n";
    }
    
    static void testGradientMappings()
    {
        std::cout << "Testing gradient mappings...\n";
        
        SpatialSampleGrid grid;
        grid.initialize(1000.0f, 600.0f);
        
        // Map entire canvas to sample 0
        for (int y = 0; y < 32; ++y)
            for (int x = 0; x < 32; ++x)
                grid.mapRegionToSampleSlot(x, y, 0);
        
        // Apply vertical gradient (pitch)
        grid.mapVerticalGradient(0, 24.0f);
        
        // Test gradient values
        auto infoTop = grid.getSampleTriggerInfo(500, 50);    // Near top
        auto infoBottom = grid.getSampleTriggerInfo(500, 550); // Near bottom
        
        assert(infoTop.pitchOffset != infoBottom.pitchOffset);
        assert(std::abs(infoTop.pitchOffset - infoBottom.pitchOffset) > 10.0f); // Should be significant difference
        
        std::cout << "✓ Gradient mappings working\n";
    }
    
    static void testPerformanceMetrics()
    {
        std::cout << "Testing performance metrics...\n";
        
        SpatialSampleGrid grid;
        grid.initialize(1000.0f, 600.0f);
        grid.applyPresetMapping(static_cast<int>(SpatialSampleGrid::PresetMapping::LinearHorizontal));
        
        // Reset metrics
        grid.resetPerformanceMetrics();
        
        // Perform some lookups
        for (int i = 0; i < 100; ++i)
        {
            float x = static_cast<float>(i * 10);
            float y = 300.0f;
            grid.getSampleTriggerInfo(x, y);
        }
        
        // Check metrics
        const auto& metrics = grid.getPerformanceMetrics();
        assert(metrics.lookupCount.load() == 100);
        assert(metrics.getCacheHitRate() >= 0.0f && metrics.getCacheHitRate() <= 1.0f);
        
        std::cout << "✓ Performance metrics working\n";
        std::cout << "  - Lookups: " << metrics.lookupCount.load() << "\n";
        std::cout << "  - Cache hit rate: " << (metrics.getCacheHitRate() * 100.0f) << "%\n";
    }
};

// Standalone test function for integration testing
void testSpatialGridIntegration()
{
    SpatialGridTest::runAllTests();
}