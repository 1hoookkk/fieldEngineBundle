// Source/Core/PresetManager.h
// Phase 2: Preset Management with JSON save/load

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_data_structures/juce_data_structures.h>
#include <memory>

// Forward declarations
class CanvasComponent;

/**
 * RT-safe preset management for SpectralCanvas Pro
 * Handles JSON serialization of APVTS + canvas state
 * All file I/O operations performed on UI thread only
 */
class PresetManager
{
public:
    PresetManager(juce::AudioProcessorValueTreeState& apvts, CanvasComponent& canvas);
    ~PresetManager() = default;
    
    //==============================================================================
    // Preset operations (UI thread only)
    bool savePreset(const juce::String& presetName, const juce::File& location);
    bool loadPreset(const juce::File& presetFile);
    
    // Get available presets from directory
    juce::StringArray getAvailablePresets(const juce::File& presetDirectory) const;
    
    // Preset validation
    bool isValidPresetFile(const juce::File& file) const;
    
    //==============================================================================
    // State serialization
    juce::var createPresetData() const;
    bool restoreFromPresetData(const juce::var& presetData);
    
    //==============================================================================
    // Canvas state integration
    struct CanvasState
    {
        int strokeCount = 0;
        juce::StringArray strokeData; // Serialized paint strokes
        int canvasWidth = 800;
        int canvasHeight = 600;
    };
    
    CanvasState captureCanvasState() const;
    void restoreCanvasState(const CanvasState& state);
    
    //==============================================================================
    // Error handling
    enum class Result
    {
        Success,
        FileNotFound,
        InvalidFormat,
        IOError,
        ValidationFailed
    };
    
    Result getLastResult() const { return lastResult; }
    juce::String getLastErrorMessage() const { return lastErrorMessage; }
    
private:
    //==============================================================================
    // Internal helpers
    juce::var serializeAPVTS() const;
    bool deserializeAPVTS(const juce::var& apvtsData);
    
    juce::var serializeCanvasData() const;
    bool deserializeCanvasData(const juce::var& canvasData);
    
    //==============================================================================
    // References to managed components
    juce::AudioProcessorValueTreeState& apvts_;
    CanvasComponent& canvas_;
    
    // Error tracking
    mutable Result lastResult = Result::Success;
    mutable juce::String lastErrorMessage;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetManager)
};