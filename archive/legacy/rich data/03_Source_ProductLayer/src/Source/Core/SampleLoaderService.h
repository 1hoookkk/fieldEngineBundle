#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <memory>

/**
 * @file SampleLoaderService.h
 * @brief Centralized async sample loading service
 * 
 * Single entry point for all sample loading operations (button, drag-drop, menu, CLI).
 * Handles file selection, decoding, error mapping, and callback management.
 * All operations are async with callbacks guaranteed on MessageThread.
 */

class SampleLoaderService
{
public:
    enum class ErrorCode
    {
        Success = 0,
        FileDialogCancelled,
        FileOpenFailed,
        UnsupportedFormat,
        DecodeFailed,
        RendererInitFailed,
        InvalidSampleData
    };

    struct LoadedSample
    {
        juce::AudioBuffer<float> audio;
        double sampleRate = 0.0;
        int numChannels = 0;
        int numSamples = 0;
        int bitDepth = 0;
        double durationSeconds = 0.0;
        juce::String filename;
        juce::String extension;
        juce::File originalFile;
    };

    struct Result
    {
        ErrorCode code = ErrorCode::Success;
        juce::String errorMessage;
        std::unique_ptr<LoadedSample> sample;
        
        bool isSuccess() const { return code == ErrorCode::Success; }
        bool hasError() const { return code != ErrorCode::Success; }
    };

    using LoadCallback = std::function<void(Result)>;

    SampleLoaderService();
    ~SampleLoaderService();

    /**
     * Launch file chooser and load selected file
     * @param parent Parent component for file chooser
     * @param callback Result callback (called on MessageThread)
     */
    void loadViaChooser(juce::Component& parent, LoadCallback callback);

    /**
     * Load specific file asynchronously  
     * @param file File to load
     * @param callback Result callback (called on MessageThread)
     */
    void loadFromFile(const juce::File& file, LoadCallback callback);

    /**
     * Get list of supported file extensions based on build configuration
     */
    juce::StringArray getSupportedExtensions() const;

    /**
     * Get human-readable supported formats string for UI
     */
    juce::String getSupportedFormatsString() const;

    /**
     * Map error code to user-friendly message
     */
    static juce::String getErrorMessage(ErrorCode code);

    /**
     * Check if extension is supported by current build
     */
    bool isExtensionSupported(const juce::String& extension) const;

private:
    class LoadOperation;
    
    void performAsyncLoad(const juce::File& file, LoadCallback callback);
    Result performSyncLoad(const juce::File& file);
    
    std::unique_ptr<juce::AudioFormatManager> formatManager_;
    std::unique_ptr<juce::ThreadPool> threadPool_;
    
    // Recent files management
    static constexpr int MAX_RECENT_FILES = 8;
    juce::StringArray recentFiles_;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SampleLoaderService)
};