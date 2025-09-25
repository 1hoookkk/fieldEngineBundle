#include "PaintEngine.h"
#include <JuceHeader.h>

/**
 * Simple test to validate PaintEngine functionality
 * This should be run during development to ensure basic operations work
 */
class PaintEngineTest
{
public:
    static bool runBasicTests()
    {
        DBG("=== PaintEngine Basic Tests ===");
        
        PaintEngine engine;
        
        // Test 1: Initialization
        if (!testInitialization(engine))
            return false;
            
        // Test 2: Audio preparation
        if (!testAudioPreparation(engine))
            return false;
            
        // Test 3: Basic stroke operations
        if (!testStrokeOperations(engine))
            return false;
            
        // Test 4: Canvas mapping functions
        if (!testCanvasMapping(engine))
            return false;
            
        // Test 5: Audio processing
        if (!testAudioProcessing(engine))
            return false;
            
        DBG("=== All PaintEngine tests passed! ===");
        return true;
    }
    
private:
    static bool testInitialization(PaintEngine& engine)
    {
        DBG("Testing initialization...");
        
        // Engine should start inactive
        if (engine.getActive())
        {
            DBG("FAIL: Engine should start inactive");
            return false;
        }
        
        // Should have zero active oscillators
        if (engine.getActiveOscillatorCount() != 0)
        {
            DBG("FAIL: Should start with zero active oscillators");
            return false;
        }
        
        // CPU load should be near zero
        if (engine.getCurrentCPULoad() > 0.1f)
        {
            DBG("FAIL: CPU load should be near zero initially");
            return false;
        }
        
        DBG("✓ Initialization test passed");
        return true;
    }
    
    static bool testAudioPreparation(PaintEngine& engine)
    {
        DBG("Testing audio preparation...");
        
        const double testSampleRate = 44100.0;
        const int testBlockSize = 512;
        
        engine.prepareToPlay(testSampleRate, testBlockSize);
        
        // After preparation, engine should still be inactive until explicitly activated
        if (engine.getActive())
        {
            DBG("FAIL: Engine should remain inactive after prepareToPlay");
            return false;
        }
        
        DBG("✓ Audio preparation test passed");
        return true;
    }
    
    static bool testStrokeOperations(PaintEngine& engine)
    {
        DBG("Testing stroke operations...");
        
        // Activate the engine
        engine.setActive(true);
        
        if (!engine.getActive())
        {
            DBG("FAIL: Engine should be active after setActive(true)");
            return false;
        }
        
        // Test basic stroke
        PaintEngine::Point startPoint(0.0f, 0.0f);
        engine.beginStroke(startPoint, 0.8f, juce::Colours::red);
        
        PaintEngine::Point midPoint(10.0f, 5.0f);
        engine.updateStroke(midPoint, 0.6f);
        
        PaintEngine::Point endPoint(20.0f, 10.0f);
        engine.updateStroke(endPoint, 0.4f);
        
        engine.endStroke();
        
        // After stroke operations, we might have some active oscillators
        // (depending on implementation details)
        
        DBG("✓ Stroke operations test passed");
        return true;
    }
    
    static bool testCanvasMapping(PaintEngine& engine)
    {
        DBG("Testing canvas mapping functions...");
        
        // Test frequency mapping
        engine.setFrequencyRange(100.0f, 1000.0f);
        
        // Test bottom of canvas maps to min frequency
        float bottomFreq = engine.canvasYToFrequency(-50.0f); // Using default canvas bounds
        if (bottomFreq < 90.0f || bottomFreq > 110.0f) // Allow some tolerance
        {
            DBG("FAIL: Bottom of canvas should map near min frequency, got " << bottomFreq);
            return false;
        }
        
        // Test top of canvas maps to max frequency  
        float topFreq = engine.canvasYToFrequency(50.0f);
        if (topFreq < 900.0f || topFreq > 1100.0f)
        {
            DBG("FAIL: Top of canvas should map near max frequency, got " << topFreq);
            return false;
        }
        
        // Test round-trip conversion
        float testFreq = 440.0f;
        float canvasY = engine.frequencyToCanvasY(testFreq);
        float backToFreq = engine.canvasYToFrequency(canvasY);
        
        if (std::abs(testFreq - backToFreq) > 1.0f)
        {
            DBG("FAIL: Round-trip frequency conversion failed: " << testFreq << " -> " << canvasY << " -> " << backToFreq);
            return false;
        }
        
        DBG("✓ Canvas mapping test passed");
        return true;
    }
    
    static bool testAudioProcessing(PaintEngine& engine)
    {
        DBG("Testing audio processing...");
        
        // Create a test audio buffer
        const int numChannels = 2;
        const int numSamples = 256;
        juce::AudioBuffer<float> testBuffer(numChannels, numSamples);
        
        // Clear the buffer
        testBuffer.clear();
        
        // Process the buffer
        engine.processBlock(testBuffer);
        
        // Check that processing completed without crash
        // (The actual audio content will depend on active strokes)
        
        // Check that buffer contains valid audio data (not NaN or infinite)
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* channelData = testBuffer.getReadPointer(ch);
            for (int sample = 0; sample < numSamples; ++sample)
            {
                if (!std::isfinite(channelData[sample]))
                {
                    DBG("FAIL: Audio buffer contains invalid values at channel " << ch << ", sample " << sample);
                    return false;
                }
            }
        }
        
        DBG("✓ Audio processing test passed");
        return true;
    }
};

// Function to run tests (can be called from main application for validation)
bool testPaintEngine()
{
    return PaintEngineTest::runBasicTests();
}