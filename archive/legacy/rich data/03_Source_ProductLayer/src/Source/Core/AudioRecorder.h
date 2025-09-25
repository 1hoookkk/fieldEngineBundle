#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <memory>

/**
 * AudioRecorder - Real-time audio capture for ARTEFACT
 * 
 * Captures output from PaintEngine and exports to WAV/AIFF formats.
 * Thread-safe design for real-time audio recording with lock-free circular buffer.
 * 
 * Features:
 * - Lock-free circular buffer for real-time capture
 * - WAV and AIFF export formats  
 * - Configurable sample rate and bit depth
 * - Performance monitoring and overflow detection
 * - Terminal-style status reporting
 */
class AudioRecorder
{
public:
    //==============================================================================
    // Core Types
    
    enum class RecordingState
    {
        Stopped,
        Recording,
        Stopping,
        Error
    };
    
    enum class ExportFormat
    {
        WAV_16bit,
        WAV_24bit,
        WAV_32bit_Float,
        AIFF_16bit,
        AIFF_24bit
    };
    
    struct RecordingInfo
    {
        RecordingState state = RecordingState::Stopped;
        juce::int64 recordedSamples = 0;
        double recordedSeconds = 0.0;
        float bufferUsagePercent = 0.0f;
        int bufferOverruns = 0;
        
        RecordingInfo() = default;
    };
    
    //==============================================================================
    // Main Interface
    
    AudioRecorder();
    ~AudioRecorder();
    
    // Audio processing lifecycle
    void prepareToPlay(double sampleRate, int samplesPerBlock, int numChannels = 2);
    void processBlock(const juce::AudioBuffer<float>& inputBuffer);
    void releaseResources();
    
    // Recording control
    bool startRecording();
    void stopRecording();
    void clearBuffer();
    
    // Export functionality
    bool exportToFile(const juce::File& outputFile, ExportFormat format = ExportFormat::WAV_24bit);
    bool exportCurrentRecording(const juce::String& filename, ExportFormat format = ExportFormat::WAV_24bit);
    
    // Configuration
    void setMaxRecordingTime(double maxSeconds) { maxRecordingTimeSeconds = maxSeconds; }
    void setBufferSize(int numSamples);
    void setRecordingDirectory(const juce::File& directory) { recordingDirectory = directory; }
    
    // Status and monitoring
    RecordingInfo getRecordingInfo() const;
    RecordingState getState() const { return currentState.load(); }
    bool isRecording() const { return currentState.load() == RecordingState::Recording; }
    double getRecordedSeconds() const;
    float getBufferUsagePercent() const;
    
    // File management
    juce::String generateTimestampedFilename(const juce::String& baseName = "ARTEFACT_Recording") const;
    juce::Array<juce::File> getRecentRecordings() const;
    
    // SAFETY: Preparation guard
    bool prepared() const { return isPrepared.load(std::memory_order_acquire); }
    
private:
    //==============================================================================
    // Lock-free Circular Buffer Implementation
    
    class CircularBuffer
    {
    public:
        CircularBuffer();
        ~CircularBuffer();
        
        void setSize(int numChannels, int numSamples);
        void clear();
        
        // Thread-safe write (called from audio thread)
        void writeBlock(const juce::AudioBuffer<float>& source, int startSample, int numSamples);
        
        // Thread-safe read (called from export thread)
        void readBlock(juce::AudioBuffer<float>& destination, juce::int64 startSample, int numSamples) const;
        
        juce::int64 getWritePosition() const { return writePosition.load(); }
        juce::int64 getAvailableSamples() const;
        int getNumChannels() const { return numChannels; }
        bool hasOverrun() const { return hasOverrunFlag.load(); }
        void clearOverrunFlag() { hasOverrunFlag.store(false); }
        
    private:
        std::unique_ptr<juce::AudioBuffer<float>> buffer;
        std::atomic<juce::int64> writePosition{ 0 };
        std::atomic<bool> hasOverrunFlag{ false };
        int numChannels = 2;
        int bufferSize = 0;
        
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CircularBuffer)
    };
    
    //==============================================================================
    // Export Threading
    
    class ExportThread : public juce::Thread
    {
    public:
        ExportThread(AudioRecorder& owner);
        ~ExportThread() override;
        
        void exportBuffer(const juce::File& file, ExportFormat format, 
                         juce::int64 startSample, juce::int64 numSamples);
        
        void run() override;
        
    private:
        AudioRecorder& recorder;
        juce::File pendingFile;
        ExportFormat pendingFormat = ExportFormat::WAV_24bit;
        juce::int64 pendingStartSample = 0;
        juce::int64 pendingNumSamples = 0;
        std::atomic<bool> hasExportTask{ false };
        
        bool writeBufferToFile(const juce::File& file, ExportFormat format, 
                              juce::int64 startSample, juce::int64 numSamples);
        std::unique_ptr<juce::AudioFormatWriter> createWriter(const juce::File& file, ExportFormat format);
        
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ExportThread)
    };
    
    //==============================================================================
    // Member Variables
    
    // Audio state
    std::atomic<RecordingState> currentState{ RecordingState::Stopped };
    double sampleRate = 44100.0;
    int samplesPerBlock = 512;
    int numChannels = 2;
    
    // Recording state
    std::atomic<juce::int64> totalRecordedSamples{ 0 };
    juce::int64 recordingStartSample = 0;
    double maxRecordingTimeSeconds = 600.0; // 10 minutes default
    
    // Buffer management
    CircularBuffer circularBuffer;
    static constexpr int DEFAULT_BUFFER_SECONDS = 60; // 1 minute default buffer
    
    // File management
    juce::File recordingDirectory;
    std::unique_ptr<ExportThread> exportThread;
    
    // Performance monitoring
    std::atomic<int> bufferOverrunCount{ 0 };
    juce::uint32 lastOverrunTime = 0;
    
    // SAFETY: Track initialization state
    std::atomic<bool> isPrepared{false};
    
    //==============================================================================
    // Private Methods
    
    void updateRecordingState();
    bool ensureRecordingDirectory();
    juce::String formatDuration(double seconds) const;
    void logRecordingEvent(const juce::String& message) const;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioRecorder)
};