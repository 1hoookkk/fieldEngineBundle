#pragma once
#include <JuceHeader.h>
#include "../../Analog/CEM3389Filter/CEM3389Filter.h"

/**
 * VintageCanvas - Paint-Controlled Analog Processor
 * 
 * Simple audio processor that applies analog warmth based on paint gestures.
 * Uses the secret CEM3389Filter for authentic EMU Audity character.
 */
class VintageCanvasProcessor : public juce::AudioProcessor
{
public:
    VintageCanvasProcessor();
    ~VintageCanvasProcessor() override;

    //==============================================================================
    // AudioProcessor Implementation
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    // Plugin Info
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    //==============================================================================
    // Programs
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int index) override { juce::ignoreUnused(index); }
    const juce::String getProgramName(int index) override { juce::ignoreUnused(index); return {}; }
    void changeProgramName(int index, const juce::String& newName) override { juce::ignoreUnused(index, newName); }

    //==============================================================================
    // State
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    //==============================================================================
    // Editor
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    //==============================================================================
    // Paint Interface
    
    /**
     * Called when user paints on the canvas
     * @param x Normalized X position (0-1)
     * @param y Normalized Y position (0-1)  
     * @param pressure Paint pressure (0-1)
     * @param velocity Paint velocity (0-1)
     * @param color Paint color
     */
    void onPaintGesture(float x, float y, float pressure, float velocity, juce::Colour color);
    
    /**
     * Clear all paint and reset filter to neutral
     */
    void clearCanvas();
    
    /**
     * Load a warmth preset
     */
    void loadPreset(const juce::String& presetName);

    //==============================================================================
    // Parameter Access (for editor)
    
    float getGlobalWarmth() const { return globalWarmth.load(); }
    void setGlobalWarmth(float warmth) { globalWarmth.store(juce::jlimit(0.0f, 1.0f, warmth)); }
    
    float getBrushSize() const { return brushSize.load(); }
    void setBrushSize(float size) { brushSize.store(juce::jlimit(0.1f, 1.0f, size)); }
    
    juce::String getCurrentPreset() const { return currentPreset; }

private:
    //==============================================================================
    // Core Components
    
    // The secret sauce - EMU Audity analog modeling
    std::unique_ptr<CEM3389Filter> analogFilter;
    
    //==============================================================================
    // Paint State
    
    // Global parameters
    std::atomic<float> globalWarmth{0.5f};      // Master warmth amount
    std::atomic<float> brushSize{0.3f};         // Paint brush size
    std::atomic<float> dryWetMix{1.0f};         // Always 100% wet for simplicity
    
    // Paint canvas state (simplified - just track last paint gesture)
    std::atomic<float> lastPaintPressure{0.0f};
    std::atomic<float> lastPaintVelocity{0.0f};
    std::atomic<float> lastPaintHue{0.5f};
    std::atomic<float> lastPaintSaturation{0.5f};
    std::atomic<float> lastPaintBrightness{0.5f};
    
    // Current preset
    juce::String currentPreset = "Default";
    
    //==============================================================================
    // Parameter Mapping
    
    void updateFilterFromPaint();
    void applyPresetSettings(const juce::String& preset);
    
    //==============================================================================
    // Presets
    
    struct WarmthPreset
    {
        juce::String name;
        float baseCutoff;
        float baseResonance;  
        float baseSaturation;
        float modulation;
        juce::String description;
    };
    
    std::vector<WarmthPreset> presets;
    void initializePresets();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VintageCanvasProcessor)
};