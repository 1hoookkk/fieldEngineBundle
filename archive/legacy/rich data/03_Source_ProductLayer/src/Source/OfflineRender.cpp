/**
 * OfflineRender.cpp
 * 
 * Standalone console application to generate demo audio files
 * showing SpectralCanvas Pro's paint-to-audio transformation.
 * 
 * Generates:
 * - before_magic.wav: Clean synthesis (magic=0.0)
 * - after_magic.wav: Full vintage character (magic=1.0)
 */

#include <JuceHeader.h>
#include "Core/SpectralSynthEngine.h"
#include "Core/TapeSpeed.h"
#include "Core/StereoWidth.h"
#include "Core/AtomicOscillator.h"
#include "Core/ColorToSpectralMapper.h"
#include "Core/PaintQueue.h"

class OfflineRenderer
{
public:
    OfflineRenderer()
    {
        // Initialize format manager for WAV writing
        formatManager.registerBasicFormats();
        
        std::cout << "SpectralCanvas Pro - OfflineRender Demo Generator" << std::endl;
        std::cout << "================================================" << std::endl;
    }
    
    bool generateDemoFiles()
    {
        const double sampleRate = 44100.0;
        const int blockSize = 512;
        const int numChannels = 2;
        const int durationSeconds = 10;
        const int totalSamples = (int)(sampleRate * durationSeconds);
        
        std::cout << "Configuration:" << std::endl;
        std::cout << "  Sample Rate: " << sampleRate << " Hz" << std::endl;
        std::cout << "  Duration: " << durationSeconds << " seconds" << std::endl;
        std::cout << "  Total Samples: " << totalSamples << std::endl;
        std::cout << std::endl;
        
        // Generate clean synthesis demo
        if (!renderDemo("before_magic.wav", false, sampleRate, blockSize, numChannels, totalSamples))
        {
            std::cerr << "Failed to generate before_magic.wav" << std::endl;
            return false;
        }
        
        // Generate vintage character demo  
        if (!renderDemo("after_magic.wav", true, sampleRate, blockSize, numChannels, totalSamples))
        {
            std::cerr << "Failed to generate after_magic.wav" << std::endl;
            return false;
        }
        
        std::cout << std::endl;
        std::cout << "✅ Demo files generated successfully!" << std::endl;
        std::cout << "   📁 before_magic.wav - Clean synthesis" << std::endl;
        std::cout << "   📁 after_magic.wav - Vintage character" << std::endl;
        
        return true;
    }

private:
    juce::AudioFormatManager formatManager;
    
    struct PaintStroke
    {
        float x, y;         // 0..1 canvas coordinates
        float pressure;     // 0..1 
        float timeSeconds;  // When to trigger
        juce::Colour color;
        
        PaintStroke(float x_, float y_, float pressure_, float time_, juce::Colour color_)
            : x(x_), y(y_), pressure(pressure_), timeSeconds(time_), color(color_) {}
    };
    
    std::vector<PaintStroke> createDemoPaintStrokes()
    {
        std::vector<PaintStroke> strokes;
        strokes.reserve(16);
        
        // High-frequency hihat-style content (75-95% of canvas height)
        strokes.emplace_back(0.1f, 0.85f, 0.9f, 0.5f, juce::Colours::red);
        strokes.emplace_back(0.3f, 0.90f, 0.7f, 1.2f, juce::Colours::orange);
        strokes.emplace_back(0.7f, 0.88f, 0.8f, 2.8f, juce::Colours::yellow);
        strokes.emplace_back(0.9f, 0.75f, 0.6f, 4.1f, juce::Colours::red);
        
        // Mid-frequency melodic content (35-65% of canvas height)
        strokes.emplace_back(0.2f, 0.55f, 0.6f, 1.8f, juce::Colours::blue);
        strokes.emplace_back(0.4f, 0.45f, 0.7f, 3.2f, juce::Colours::cyan);
        strokes.emplace_back(0.6f, 0.60f, 0.5f, 5.5f, juce::Colours::green);
        strokes.emplace_back(0.8f, 0.40f, 0.8f, 7.1f, juce::Colours::blue);
        
        // Low-frequency foundation (10-30% of canvas height)
        strokes.emplace_back(0.15f, 0.25f, 0.9f, 0.8f, juce::Colours::purple);
        strokes.emplace_back(0.35f, 0.20f, 0.7f, 2.3f, juce::Colours::magenta);
        strokes.emplace_back(0.55f, 0.15f, 0.8f, 4.7f, juce::Colours::darkviolet);
        strokes.emplace_back(0.75f, 0.30f, 0.6f, 6.9f, juce::Colours::purple);
        
        // Sweeping animated content
        strokes.emplace_back(0.05f, 0.70f, 0.4f, 3.5f, juce::Colours::white);
        strokes.emplace_back(0.25f, 0.65f, 0.5f, 6.2f, juce::Colours::lightgrey);
        strokes.emplace_back(0.45f, 0.50f, 0.6f, 8.1f, juce::Colours::white);
        strokes.emplace_back(0.85f, 0.35f, 0.3f, 9.2f, juce::Colours::silver);
        
        return strokes;
    }
    
    bool renderDemo(const juce::String& filename, bool magicEnabled, 
                   double sampleRate, int blockSize, int numChannels, int totalSamples)
    {
        std::cout << "Rendering: " << filename << " (magic: " << (magicEnabled ? "ON" : "OFF") << ")" << std::endl;
        
        // Create engines
        auto& spectralEngine = SpectralSynthEngine::instance();
        TapeSpeed tapeSpeed;
        StereoWidth stereoWidth;
        
        // Prepare engines
        spectralEngine.prepare(sampleRate, blockSize);
        tapeSpeed.prepareToPlay(sampleRate, blockSize);
        stereoWidth.prepareToPlay(sampleRate, blockSize);
        
        // Configure vintage parameters if magic is enabled
        if (magicEnabled)
        {
            // Vintage tape character
            tapeSpeed.setSpeedRatio(1.03f);      // Slightly faster (3% speed up)
            tapeSpeed.setWowFlutter(0.3f);       // Moderate wow/flutter
            tapeSpeed.setMode(1);                // Dynamic mode for character
            
            // Wide stereo image
            stereoWidth.setWidth(1.4f);          // 40% wider stereo field
        }
        else
        {
            // Clean settings
            tapeSpeed.setSpeedRatio(1.0f);       // Normal speed
            tapeSpeed.setWowFlutter(0.0f);       // No wow/flutter
            tapeSpeed.setMode(0);                // Fixed mode
            
            stereoWidth.setWidth(1.0f);          // Normal stereo width
        }
        
        // Create output file
        juce::File outputFile = juce::File::getCurrentWorkingDirectory().getChildFile(filename);
        outputFile.deleteFile(); // Remove if exists
        
        // Get WAV format and create writer
        auto* wavFormat = formatManager.getDefaultFormat();
        auto* writer = wavFormat ? wavFormat->createWriterFor(new juce::FileOutputStream(outputFile),
                                                    sampleRate, 
                                                    numChannels, 
                                                    16, // 16-bit
                                                    {},
                                                    0) : nullptr;
        
        if (writer == nullptr)
        {
            std::cerr << "Failed to create writer for " << filename.toRawUTF8() << std::endl;
            return false;
        }
        
        std::unique_ptr<juce::AudioFormatWriter> writerPtr(writer);
        
        // Generate demo paint strokes
        auto paintStrokes = createDemoPaintStrokes();
        
        // Audio processing buffers
        juce::AudioBuffer<float> renderBuffer(numChannels, blockSize);
        juce::AudioBuffer<float> tempBuffer(numChannels, blockSize);
        
        int samplesProcessed = 0;
        int progressPercent = -1;
        
        // Main rendering loop
        while (samplesProcessed < totalSamples)
        {
            const int samplesToProcess = juce::jmin(blockSize, totalSamples - samplesProcessed);
            const double currentTimeSeconds = samplesProcessed / sampleRate;
            
            // Update progress
            int newPercent = (int)(100.0 * samplesProcessed / totalSamples);
            if (newPercent != progressPercent && newPercent % 10 == 0)
            {
                progressPercent = newPercent;
                std::cout << "  Progress: " << progressPercent << "%" << std::endl;
            }
            
            // Clear render buffer
            renderBuffer.clear();
            
            // Process any paint strokes that should trigger at this time
            for (const auto& stroke : paintStrokes)
            {
                const double strokeStartTime = stroke.timeSeconds;
                const double strokeEndTime = strokeStartTime + 0.1; // 100ms stroke duration
                
                if (currentTimeSeconds >= strokeStartTime && currentTimeSeconds < strokeEndTime)
                {
                    // Trigger paint stroke in spectral engine
                    // Convert stroke to PaintEvent and push to engine
                    PaintEvent event(stroke.x / 1000.0f, stroke.y / 1000.0f, stroke.pressure, kStrokeMove);
                    spectralEngine.pushGestureRT(event);
                }
            }
            
            // Generate spectral synthesis
            spectralEngine.processAudioBlock(renderBuffer, sampleRate);
            
            // Apply vintage processing if enabled
            if (magicEnabled)
            {
                // Copy to temp buffer for tape speed processing
                tempBuffer.clear();
                for (int ch = 0; ch < numChannels; ++ch)
                {
                    tempBuffer.copyFrom(ch, 0, renderBuffer, ch, 0, samplesToProcess);
                }
                
                // Apply tape speed character
                tapeSpeed.processBlock(tempBuffer);
                
                // Apply stereo width processing
                stereoWidth.processBlock(tempBuffer);
                
                // Copy processed audio back
                for (int ch = 0; ch < numChannels; ++ch)
                {
                    renderBuffer.copyFrom(ch, 0, tempBuffer, ch, 0, samplesToProcess);
                }
            }
            else
            {
                // Clean processing - just apply normal stereo width
                stereoWidth.processBlock(renderBuffer);
            }
            
            // Write to file
            writerPtr->writeFromAudioSampleBuffer(renderBuffer, 0, samplesToProcess);
            
            samplesProcessed += samplesToProcess;
        }
        
        // Cleanup
        writerPtr.reset();
        
        spectralEngine.releaseResources();
        
        std::cout << "  ✅ Completed: " << filename << " (" << (outputFile.getSize() / 1024) << " KB)" << std::endl;
        return true;
    }
};

int main()
{
    // Initialize JUCE
    juce::initialiseJuce_GUI();
    
    int result = 0;
    
    try
    {
        OfflineRenderer renderer;
        if (!renderer.generateDemoFiles())
        {
            result = 1;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        result = 1;
    }
    catch (...)
    {
        std::cerr << "Unknown error occurred" << std::endl;
        result = 1;
    }
    
    // Cleanup JUCE
    juce::shutdownJuce_GUI();
    
    return result;
}