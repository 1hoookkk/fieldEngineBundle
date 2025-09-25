#include "PluginProcessor.h"
#include "PaintEngine.h"
#include "Commands.h"
#include <JuceHeader.h>

/**
 * Integration test for PaintEngine + PluginProcessor
 * Validates that the complete command flow works end-to-end
 */
class IntegrationTest
{
public:
    static bool runIntegrationTests()
    {
        DBG("=== SoundCanvas Integration Tests ===");
        
        // Test 1: Plugin processor creation
        if (!testPluginProcessorCreation())
            return false;
            
        // Test 2: Command queue integration
        if (!testCommandQueueIntegration())
            return false;
            
        // Test 3: Paint engine integration
        if (!testPaintEngineIntegration())
            return false;
            
        // Test 4: Audio processing integration
        if (!testAudioProcessingIntegration())
            return false;
            
        DBG("=== All Integration Tests Passed! ===");
        return true;
    }
    
private:
    static bool testPluginProcessorCreation()
    {
        DBG("Testing plugin processor creation...");
        
        try
        {
            auto processor = std::make_unique<ARTEFACTAudioProcessor>();
            
            // Basic validation
            if (processor->getName().isEmpty())
            {
                DBG("FAIL: Plugin name is empty");
                return false;
            }
            
            if (!processor->hasEditor())
            {
                DBG("FAIL: Plugin should have an editor");
                return false;
            }
            
            if (!processor->acceptsMidi())
            {
                DBG("FAIL: Plugin should accept MIDI");
                return false;
            }
        }
        catch (const std::exception& e)
        {
            DBG("FAIL: Exception during processor creation: " << e.what());
            return false;
        }
        
        DBG("✓ Plugin processor creation test passed");
        return true;
    }
    
    static bool testCommandQueueIntegration()
    {
        DBG("Testing command queue integration...");
        
        try
        {
            auto processor = std::make_unique<ARTEFACTAudioProcessor>();
            
            // Test Forge command
            Command forgeCmd(ForgeCommandID::SetVolume, 0, 0.8f);
            bool forgeResult = processor->pushCommandToQueue(forgeCmd);
            
            if (!forgeResult)
            {
                DBG("FAIL: Failed to push Forge command to queue");
                return false;
            }
            
            // Test Paint command
            Command paintCmd(PaintCommandID::BeginStroke, 10.0f, 20.0f, 0.7f, juce::Colours::blue);
            bool paintResult = processor->pushCommandToQueue(paintCmd);
            
            if (!paintResult)
            {
                DBG("FAIL: Failed to push Paint command to queue");
                return false;
            }
            
            // Verify command type detection
            if (!forgeCmd.isForgeCommand() || forgeCmd.isPaintCommand())
            {
                DBG("FAIL: Forge command type detection failed");
                return false;
            }
            
            if (!paintCmd.isPaintCommand() || paintCmd.isForgeCommand())
            {
                DBG("FAIL: Paint command type detection failed");
                return false;
            }
        }
        catch (const std::exception& e)
        {
            DBG("FAIL: Exception during command queue test: " << e.what());
            return false;
        }
        
        DBG("✓ Command queue integration test passed");
        return true;
    }
    
    static bool testPaintEngineIntegration()
    {
        DBG("Testing paint engine integration...");
        
        try
        {
            auto processor = std::make_unique<ARTEFACTAudioProcessor>();
            
            // Access paint engine through processor
            auto& paintEngine = processor->getPaintEngine();
            
            // Test basic paint engine operations
            paintEngine.setActive(true);
            
            if (!paintEngine.getActive())
            {
                DBG("FAIL: Paint engine should be active after setActive(true)");
                return false;
            }
            
            // Test frequency mapping
            paintEngine.setFrequencyRange(100.0f, 1000.0f);
            float testFreq = paintEngine.canvasYToFrequency(0.0f);
            
            if (testFreq < 90.0f || testFreq > 110.0f)
            {
                DBG("FAIL: Frequency mapping test failed, got " << testFreq);
                return false;
            }
        }
        catch (const std::exception& e)
        {
            DBG("FAIL: Exception during paint engine test: " << e.what());
            return false;
        }
        
        DBG("✓ Paint engine integration test passed");
        return true;
    }
    
    static bool testAudioProcessingIntegration()
    {
        DBG("Testing audio processing integration...");
        
        try
        {
            auto processor = std::make_unique<ARTEFACTAudioProcessor>();
            
            // Prepare for audio processing
            const double testSampleRate = 44100.0;
            const int testBlockSize = 256;
            
            processor->prepareToPlay(testSampleRate, testBlockSize);
            
            // Create test audio buffer
            juce::AudioBuffer<float> testBuffer(2, testBlockSize);
            juce::MidiBuffer testMidi;
            
            testBuffer.clear();
            
            // Process audio block (should not crash)
            processor->processBlock(testBuffer, testMidi);
            
            // Verify buffer contains valid audio data
            for (int ch = 0; ch < testBuffer.getNumChannels(); ++ch)
            {
                auto* channelData = testBuffer.getReadPointer(ch);
                for (int sample = 0; sample < testBlockSize; ++sample)
                {
                    if (!std::isfinite(channelData[sample]))
                    {
                        DBG("FAIL: Audio buffer contains invalid values");
                        return false;
                    }
                }
            }
            
            // Test mode switching
            Command modeCmd(PaintCommandID::SetPaintActive, true);
            processor->pushCommandToQueue(modeCmd);
            
            // Process another block
            processor->processBlock(testBuffer, testMidi);
            
            processor->releaseResources();
        }
        catch (const std::exception& e)
        {
            DBG("FAIL: Exception during audio processing test: " << e.what());
            return false;
        }
        
        DBG("✓ Audio processing integration test passed");
        return true;
    }
};

// External function to run integration tests
bool testSoundCanvasIntegration()
{
    return IntegrationTest::runIntegrationTests();
}