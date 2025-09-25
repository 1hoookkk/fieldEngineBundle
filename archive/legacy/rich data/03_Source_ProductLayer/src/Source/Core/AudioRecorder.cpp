#include "AudioRecorder.h"
#include <cmath>
#include <algorithm>

//==============================================================================
// AudioRecorder Implementation

AudioRecorder::AudioRecorder()
{
    // Set default recording directory to user's music folder
    auto musicDir = juce::File::getSpecialLocation(juce::File::userMusicDirectory);
    recordingDirectory = musicDir.getChildFile("ARTEFACT_Recordings");
    
    // Create export thread but DO NOT start it yet
    // Thread will be started in prepareToPlay() to avoid race conditions during plugin init
    exportThread = std::make_unique<ExportThread>(*this);
    
    logRecordingEvent("AudioRecorder initialized");
}

AudioRecorder::~AudioRecorder()
{
    stopRecording();
    
    if (exportThread)
    {
        exportThread->signalThreadShouldExit();
        exportThread->waitForThreadToExit(2000);
        exportThread.reset();
    }
    
    releaseResources();
}

//==============================================================================
// Audio Processing Lifecycle

void AudioRecorder::prepareToPlay(double sr, int samplesPerBlock_, int numChannels_)
{
    sampleRate = sr;
    samplesPerBlock = samplesPerBlock_;
    numChannels = numChannels_;
    
    // Start export thread if not already running
    // Deferred from constructor to avoid race conditions during plugin initialization
    if (exportThread && !exportThread->isThreadRunning())
    {
        exportThread->startThread(juce::Thread::Priority::normal);
    }
    
    // Calculate buffer size based on max recording time
    const int bufferSamples = static_cast<int>(sampleRate * maxRecordingTimeSeconds);
    circularBuffer.setSize(numChannels, bufferSamples);
    
    // SAFETY: Mark engine as properly initialized
    isPrepared.store(true, std::memory_order_release);
    
    logRecordingEvent(juce::String("AudioRecorder prepared: ") + 
                     juce::String(sampleRate, 1) + "Hz, " + 
                     juce::String(numChannels) + " channels, " +
                     juce::String(maxRecordingTimeSeconds, 1) + "s buffer");
}

void AudioRecorder::processBlock(const juce::AudioBuffer<float>& inputBuffer)
{
    if (!prepared()) return;  // SAFETY: Guard against pre-init calls
    
    if (currentState.load() != RecordingState::Recording)
        return;
        
    const int numSamples = inputBuffer.getNumSamples();
    const int inputChannels = inputBuffer.getNumChannels();
    
    // Safety check
    if (numSamples <= 0 || inputChannels <= 0)
        return;
    
    // Write to circular buffer - this is lock-free and real-time safe
    circularBuffer.writeBlock(inputBuffer, 0, numSamples);
    
    // Update recorded sample count
    totalRecordedSamples.fetch_add(numSamples);
    
    // Check for buffer overruns
    if (circularBuffer.hasOverrun())
    {
        bufferOverrunCount.fetch_add(1);
        circularBuffer.clearOverrunFlag();
        lastOverrunTime = juce::Time::getMillisecondCounter();
        
        // RT-SAFE: Removed logging from audio thread (overrun detection still active)
    }
    
    // Auto-stop if we've reached max recording time
    const double recordedSeconds = getRecordedSeconds();
    if (recordedSeconds >= maxRecordingTimeSeconds)
    {
        currentState.store(RecordingState::Stopping);
        // RT-SAFE: Removed logging from audio thread (auto-stop still functional)
    }
}

void AudioRecorder::releaseResources()
{
    stopRecording();
    circularBuffer.clear();
}

//==============================================================================
// Recording Control

bool AudioRecorder::startRecording()
{
    if (currentState.load() == RecordingState::Recording)
        return true; // Already recording
        
    if (!ensureRecordingDirectory())
    {
        currentState.store(RecordingState::Error);
        logRecordingEvent("Error: Could not create recording directory");
        return false;
    }
    
    // Clear buffer and reset counters
    circularBuffer.clear();
    totalRecordedSamples.store(0);
    recordingStartSample = circularBuffer.getWritePosition();
    bufferOverrunCount.store(0);
    
    currentState.store(RecordingState::Recording);
    logRecordingEvent("Recording started");
    
    return true;
}

void AudioRecorder::stopRecording()
{
    if (currentState.load() == RecordingState::Stopped)
        return; // Already stopped
        
    currentState.store(RecordingState::Stopping);
    
    // RT-SAFE: Use atomic state instead of blocking sleep
    // Audio thread checks RecordingState::Stopping and handles cleanup
    currentState.store(RecordingState::Stopped);
    
    const double duration = getRecordedSeconds();
    logRecordingEvent(juce::String("Recording stopped. Duration: ") + formatDuration(duration));
}

void AudioRecorder::clearBuffer()
{
    circularBuffer.clear();
    totalRecordedSamples.store(0);
    bufferOverrunCount.store(0);
    logRecordingEvent("Recording buffer cleared");
}

//==============================================================================
// Export Functionality

bool AudioRecorder::exportToFile(const juce::File& outputFile, ExportFormat format)
{
    if (currentState.load() == RecordingState::Recording)
    {
        logRecordingEvent("Warning: Cannot export while recording");
        return false;
    }
    
    const juce::int64 samplesRecorded = totalRecordedSamples.load();
    if (samplesRecorded <= 0)
    {
        logRecordingEvent("Warning: No audio data to export");
        return false;
    }
    
    // Start async export
    exportThread->exportBuffer(outputFile, format, recordingStartSample, samplesRecorded);
    logRecordingEvent("Export started: " + outputFile.getFileName());
    
    return true;
}

bool AudioRecorder::exportCurrentRecording(const juce::String& filename, ExportFormat format)
{
    if (!ensureRecordingDirectory())
        return false;
        
    juce::String actualFilename = filename;
    if (actualFilename.isEmpty())
        actualFilename = generateTimestampedFilename();
        
    // Add extension based on format
    if (!actualFilename.contains("."))
    {
        switch (format)
        {
            case ExportFormat::WAV_16bit:
            case ExportFormat::WAV_24bit:
            case ExportFormat::WAV_32bit_Float:
                actualFilename += ".wav";
                break;
            case ExportFormat::AIFF_16bit:
            case ExportFormat::AIFF_24bit:
                actualFilename += ".aiff";
                break;
        }
    }
    
    auto outputFile = recordingDirectory.getChildFile(actualFilename);
    return exportToFile(outputFile, format);
}

//==============================================================================
// Status and Monitoring

AudioRecorder::RecordingInfo AudioRecorder::getRecordingInfo() const
{
    RecordingInfo info;
    info.state = currentState.load();
    info.recordedSamples = totalRecordedSamples.load();
    info.recordedSeconds = getRecordedSeconds();
    info.bufferUsagePercent = getBufferUsagePercent();
    info.bufferOverruns = bufferOverrunCount.load();
    
    return info;
}

double AudioRecorder::getRecordedSeconds() const
{
    if (sampleRate <= 0.0)
        return 0.0;
        
    return static_cast<double>(totalRecordedSamples.load()) / sampleRate;
}

float AudioRecorder::getBufferUsagePercent() const
{
    const juce::int64 availableSamples = circularBuffer.getAvailableSamples();
    const juce::int64 maxSamples = static_cast<juce::int64>(sampleRate * maxRecordingTimeSeconds);
    
    if (maxSamples <= 0)
        return 0.0f;
        
    return static_cast<float>(availableSamples) / static_cast<float>(maxSamples) * 100.0f;
}

//==============================================================================
// File Management

juce::String AudioRecorder::generateTimestampedFilename(const juce::String& baseName) const
{
    const auto now = juce::Time::getCurrentTime();
    const juce::String timestamp = now.formatted("%Y%m%d_%H%M%S");
    return baseName + "_" + timestamp;
}

juce::Array<juce::File> AudioRecorder::getRecentRecordings() const
{
    juce::Array<juce::File> recordings;
    
    if (!recordingDirectory.exists())
        return recordings;
        
    juce::Array<juce::File> files;
    recordingDirectory.findChildFiles(files, juce::File::findFiles, false, "*.wav;*.aiff", juce::File::FollowSymlinks::yes);
    
    // Sort by modification time (newest first)
    std::sort(files.begin(), files.end(), [](const juce::File& a, const juce::File& b)
    {
        return a.getLastModificationTime() > b.getLastModificationTime();
    });
    
    return files;
}

//==============================================================================
// Configuration

void AudioRecorder::setBufferSize(int numSamples)
{
    if (currentState.load() == RecordingState::Recording)
    {
        logRecordingEvent("Warning: Cannot change buffer size while recording");
        return;
    }
    
    if (numSamples > 0 && sampleRate > 0)
    {
        maxRecordingTimeSeconds = static_cast<double>(numSamples) / sampleRate;
        circularBuffer.setSize(numChannels, numSamples);
        logRecordingEvent(juce::String("Buffer size set to ") + formatDuration(maxRecordingTimeSeconds));
    }
}

//==============================================================================
// Private Methods

void AudioRecorder::updateRecordingState()
{
    // This could be called periodically to update state
    // For now, state is managed directly in other methods
}

bool AudioRecorder::ensureRecordingDirectory()
{
    if (!recordingDirectory.exists())
    {
        auto result = recordingDirectory.createDirectory();
        if (!result.wasOk())
        {
            logRecordingEvent("Error creating recording directory: " + result.getErrorMessage());
            return false;
        }
    }
    
    return recordingDirectory.exists() && recordingDirectory.isDirectory();
}

juce::String AudioRecorder::formatDuration(double seconds) const
{
    const int totalSeconds = static_cast<int>(seconds);
    const int minutes = totalSeconds / 60;
    const int secs = totalSeconds % 60;
    const int ms = static_cast<int>((seconds - totalSeconds) * 1000);
    
    return juce::String::formatted("%02d:%02d.%03d", minutes, secs, ms);
}

void AudioRecorder::logRecordingEvent(const juce::String& message) const
{
    const auto timestamp = juce::Time::getCurrentTime().formatted("[%H:%M:%S] ");
    DBG("AudioRecorder " << timestamp << message);
}

//==============================================================================
// CircularBuffer Implementation

AudioRecorder::CircularBuffer::CircularBuffer()
{
}

AudioRecorder::CircularBuffer::~CircularBuffer()
{
}

void AudioRecorder::CircularBuffer::setSize(int numChannels_, int numSamples)
{
    if (numChannels_ <= 0 || numSamples <= 0)
        return;
        
    numChannels = numChannels_;
    bufferSize = numSamples;
    
    buffer = std::make_unique<juce::AudioBuffer<float>>(numChannels, bufferSize);
    buffer->clear();
    
    writePosition.store(0);
    hasOverrunFlag.store(false);
}

void AudioRecorder::CircularBuffer::clear()
{
    if (buffer)
        buffer->clear();
        
    writePosition.store(0);
    hasOverrunFlag.store(false);
}

void AudioRecorder::CircularBuffer::writeBlock(const juce::AudioBuffer<float>& source, int startSample, int numSamples)
{
    if (!buffer || numSamples <= 0)
        return;
        
    const int sourceChannels = source.getNumChannels();
    const int channelsToWrite = juce::jmin(sourceChannels, numChannels);
    
    const juce::int64 writePos = writePosition.load();
    const int writeIndex = static_cast<int>(writePos % bufferSize);
    
    // Check if we'll wrap around
    if (writeIndex + numSamples <= bufferSize)
    {
        // Simple case - no wrap around
        for (int ch = 0; ch < channelsToWrite; ++ch)
        {
            buffer->copyFrom(ch, writeIndex, source, ch, startSample, numSamples);
        }
    }
    else
    {
        // Wrap around case
        const int samplesBeforeWrap = bufferSize - writeIndex;
        const int samplesAfterWrap = numSamples - samplesBeforeWrap;
        
        // Write first part
        for (int ch = 0; ch < channelsToWrite; ++ch)
        {
            buffer->copyFrom(ch, writeIndex, source, ch, startSample, samplesBeforeWrap);
        }
        
        // Write wrapped part
        if (samplesAfterWrap > 0)
        {
            for (int ch = 0; ch < channelsToWrite; ++ch)
            {
                buffer->copyFrom(ch, 0, source, ch, startSample + samplesBeforeWrap, samplesAfterWrap);
            }
        }
    }
    
    // Update write position
    writePosition.fetch_add(numSamples);
    
    // Check for overrun (writing faster than we can keep up)
    // Use a reasonable buffer margin (1024 samples) for overrun detection
    if (getAvailableSamples() >= bufferSize - 1024)
    {
        hasOverrunFlag.store(true);
    }
}

void AudioRecorder::CircularBuffer::readBlock(juce::AudioBuffer<float>& destination, juce::int64 startSample, int numSamples) const
{
    if (!buffer || numSamples <= 0)
        return;
        
    const int destChannels = destination.getNumChannels();
    const int channelsToRead = juce::jmin(destChannels, numChannels);
    
    const int readIndex = static_cast<int>(startSample % bufferSize);
    
    // Check if we'll wrap around
    if (readIndex + numSamples <= bufferSize)
    {
        // Simple case - no wrap around
        for (int ch = 0; ch < channelsToRead; ++ch)
        {
            destination.copyFrom(ch, 0, *buffer, ch, readIndex, numSamples);
        }
    }
    else
    {
        // Wrap around case
        const int samplesBeforeWrap = bufferSize - readIndex;
        const int samplesAfterWrap = numSamples - samplesBeforeWrap;
        
        // Read first part
        for (int ch = 0; ch < channelsToRead; ++ch)
        {
            destination.copyFrom(ch, 0, *buffer, ch, readIndex, samplesBeforeWrap);
        }
        
        // Read wrapped part
        if (samplesAfterWrap > 0)
        {
            for (int ch = 0; ch < channelsToRead; ++ch)
            {
                destination.copyFrom(ch, samplesBeforeWrap, *buffer, ch, 0, samplesAfterWrap);
            }
        }
    }
}

juce::int64 AudioRecorder::CircularBuffer::getAvailableSamples() const
{
    return writePosition.load();
}

//==============================================================================
// ExportThread Implementation

AudioRecorder::ExportThread::ExportThread(AudioRecorder& owner_)
    : juce::Thread("AudioRecorder Export"), recorder(owner_)
{
}

AudioRecorder::ExportThread::~ExportThread()
{
    signalThreadShouldExit();
    waitForThreadToExit(2000);
}

void AudioRecorder::ExportThread::exportBuffer(const juce::File& file, ExportFormat format, 
                                              juce::int64 startSample, juce::int64 numSamples)
{
    pendingFile = file;
    pendingFormat = format;
    pendingStartSample = startSample;
    pendingNumSamples = numSamples;
    hasExportTask.store(true);
    
    // Wake up the thread
    notify();
}

void AudioRecorder::ExportThread::run()
{
    while (!threadShouldExit())
    {
        if (hasExportTask.load())
        {
            // Process the export task
            const bool success = writeBufferToFile(pendingFile, pendingFormat, pendingStartSample, pendingNumSamples);
            
            if (success)
            {
                recorder.logRecordingEvent("Export completed: " + pendingFile.getFileName());
            }
            else
            {
                recorder.logRecordingEvent("Export failed: " + pendingFile.getFileName());
            }
            
            hasExportTask.store(false);
        }
        else
        {
            // Wait for export task
            wait(1000); // Wake up every second to check for exit
        }
    }
}

bool AudioRecorder::ExportThread::writeBufferToFile(const juce::File& file, ExportFormat format, 
                                                   juce::int64 startSample, juce::int64 numSamples)
{
    if (numSamples <= 0)
        return false;
        
    auto writer = createWriter(file, format);
    if (!writer)
        return false;
        
    // Export in chunks to avoid large memory allocations
    constexpr int CHUNK_SIZE = 8192;
    juce::AudioBuffer<float> tempBuffer(recorder.numChannels, CHUNK_SIZE);
    
    juce::int64 samplesRemaining = numSamples;
    juce::int64 currentSample = startSample;
    
    while (samplesRemaining > 0 && !threadShouldExit())
    {
        const int samplesToWrite = static_cast<int>(juce::jmin(samplesRemaining, static_cast<juce::int64>(CHUNK_SIZE)));
        
        // Read from circular buffer
        recorder.circularBuffer.readBlock(tempBuffer, currentSample, samplesToWrite);
        
        // Write to file
        if (!writer->writeFromAudioSampleBuffer(tempBuffer, 0, samplesToWrite))
        {
            recorder.logRecordingEvent("Error writing audio data to file");
            return false;
        }
        
        samplesRemaining -= samplesToWrite;
        currentSample += samplesToWrite;
        
        // Small yield to prevent blocking
        juce::Thread::yield();
    }
    
    writer->flush();
    return true;
}

std::unique_ptr<juce::AudioFormatWriter> AudioRecorder::ExportThread::createWriter(const juce::File& file, ExportFormat format)
{
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    
    juce::AudioFormat* audioFormat = nullptr;
    int bitsPerSample = 16;
    
    switch (format)
    {
        case ExportFormat::WAV_16bit:
            audioFormat = formatManager.findFormatForFileExtension("wav");
            bitsPerSample = 16;
            break;
        case ExportFormat::WAV_24bit:
            audioFormat = formatManager.findFormatForFileExtension("wav");
            bitsPerSample = 24;
            break;
        case ExportFormat::WAV_32bit_Float:
            audioFormat = formatManager.findFormatForFileExtension("wav");
            bitsPerSample = 32;
            break;
        case ExportFormat::AIFF_16bit:
            audioFormat = formatManager.findFormatForFileExtension("aiff");
            bitsPerSample = 16;
            break;
        case ExportFormat::AIFF_24bit:
            audioFormat = formatManager.findFormatForFileExtension("aiff");
            bitsPerSample = 24;
            break;
    }
    
    if (!audioFormat)
    {
        recorder.logRecordingEvent("Error: Unsupported audio format");
        return nullptr;
    }
    
    auto fileStream = file.createOutputStream();
    if (!fileStream)
    {
        recorder.logRecordingEvent("Error: Could not create output file stream");
        return nullptr;
    }
    
    return std::unique_ptr<juce::AudioFormatWriter>(
        audioFormat->createWriterFor(fileStream.release(), 
                                   recorder.sampleRate,
                                   recorder.numChannels,
                                   bitsPerSample,
                                   {}, 0));
}