#include "SampleMaskingEngine.h"
#include <cmath>
#include <limits>
#include <memory>

//==============================================================================
SampleMaskingEngine::SampleMaskingEngine()
{
    formatManager.registerBasicFormats();
    
    // Initialize default canvas settings for typical use
    setCanvasSize(1000.0f, 500.0f);
    setTimeRange(0.0f, 4.0f);
    
    // Create default layer
    createLayer("Main");
    
    // Initialize delay line
    delayLine.setMaxDelay(2.0, 44100.0); // 2 second max delay
    
    // Initialize spectral analyzer for tempo detection (NEW!)
    spectralAnalyzer = std::make_unique<SpectralAnalyzer>();
    
    // Set default quantization for beatmakers
    setQuantization(QuantizeGrid::Sixteenth); // 1/16 note default
    setQuantizationStrength(0.5f); // 50% snap strength

    // Initialize lock-free snapshot for audio thread consumption (C++17 portable)
    std::atomic_store_explicit(&activeMasksSnapshot, std::make_shared<const std::vector<PaintMask>>(), std::memory_order_release);
}

SampleMaskingEngine::~SampleMaskingEngine()
{
    releaseResources();
}

//==============================================================================
// Audio Processing

void SampleMaskingEngine::prepareToPlay(double sampleRate, int samplesPerBlock, int numChannels)
{
    currentSampleRate = sampleRate;
    
    // Initialize effects processors
    maskFilter.setParams(1000.0f, 0.0f, sampleRate);
    delayLine.setMaxDelay(2.0, sampleRate);
    
    // SAFETY: Mark engine as properly initialized
    isPrepared.store(true, std::memory_order_release);
}

void SampleMaskingEngine::processBlock(juce::AudioBuffer<float>& buffer)
{
    if (!prepared()) return;  // SAFETY: Guard against pre-init calls
    
    if (!hasSample() || !isPlaying.load())
    {
        buffer.clear();
        return;
    }
    
    auto startTime = juce::Time::getMillisecondCounter();
    
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    const float speed = playbackSpeed.load();
    
    // Get current playback position
    double currentPos = playbackPosition.load();
    const double sampleLength = static_cast<double>(sampleBuffer->getNumSamples());
    
    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);
        auto* sourceData = sampleBuffer->getReadPointer(juce::jmin(channel, sampleBuffer->getNumChannels() - 1));
        
        for (int sample = 0; sample < numSamples; ++sample)
        {
            // Calculate current time in seconds
            const double timeSeconds = (currentPos + sample * speed) / currentSampleRate;
            
            // Get base sample value with interpolation
            float outputSample = 0.0f;
            const double sampleIndex = currentPos + sample * speed;
            
            if (sampleIndex >= 0.0 && sampleIndex < sampleLength - 1)
            {
                const int index = static_cast<int>(sampleIndex);
                const float fraction = static_cast<float>(sampleIndex - index);
                
                // Linear interpolation
                outputSample = sourceData[index] * (1.0f - fraction) + 
                              sourceData[index + 1] * fraction;
            }
            
            // Apply all active masks
            {
                // Lock-free read of snapshot built on the UI thread
                auto snapshot = std::atomic_load_explicit(&activeMasksSnapshot, std::memory_order_acquire);
                if (snapshot)
                for (const auto& mask : *snapshot)
                {
                    if (!mask.isActive) continue;
                    
                    switch (mask.mode)
                    {
                        case MaskingMode::Volume:
                            outputSample = applyVolumeMask(mask, outputSample, timeSeconds);
                            break;
                        case MaskingMode::Filter:
                            outputSample = applyFilterMask(mask, outputSample, timeSeconds);
                            break;
                        case MaskingMode::Pitch:
                            outputSample = applyPitchMask(mask, outputSample, timeSeconds);
                            break;
                        case MaskingMode::Granular:
                            outputSample = applyGranularMask(mask, outputSample, timeSeconds);
                            break;
                        case MaskingMode::Reverse:
                            // TODO: Implement reverse playback
                            break;
                        case MaskingMode::Chop:
                            outputSample = applyChopMask(mask, outputSample, timeSeconds);
                            break;
                        case MaskingMode::Stutter:
                            outputSample = applyStutterMask(mask, outputSample, timeSeconds);
                            break;
                        case MaskingMode::Ring:
                            outputSample = applyRingMask(mask, outputSample, timeSeconds);
                            break;
                        case MaskingMode::Distortion:
                            outputSample = applyDistortionMask(mask, outputSample, timeSeconds);
                            break;
                        case MaskingMode::Delay:
                            outputSample = applyDelayMask(mask, outputSample, timeSeconds);
                            break;
                    }
                }
            }
            
            channelData[sample] = outputSample;
        }
    }
    
    // Update playback position
    currentPos += numSamples * speed;
    
    // Handle looping
    if (isLooping.load() && currentPos >= sampleLength)
    {
        currentPos = std::fmod(currentPos, sampleLength);
    }
    else if (currentPos >= sampleLength)
    {
        isPlaying.store(false);
        currentPos = 0.0;
    }
    
    playbackPosition.store(currentPos);
    
    // Update performance metrics
    auto endTime = juce::Time::getMillisecondCounter();
    auto processingTime = endTime - startTime;
    cpuUsage.store(static_cast<float>(processingTime) / (numSamples / currentSampleRate * 1000.0f));
}

void SampleMaskingEngine::releaseResources()
{
    sampleBuffer.reset();
    
    // Protect mask data structures with RAII locking
    juce::ScopedLock lock(maskLock);
    activeMasks.clear();
    maskLayers.clear();
}

//==============================================================================
// Sample Loading & Management

SampleMaskingEngine::LoadResult SampleMaskingEngine::loadSample(const juce::File& sampleFile)
{
    LoadResult result;
    result.fileName = sampleFile.getFileName();
    
    // Professional file validation
    if (!sampleFile.exists())
    {
        result.errorMessage = "File does not exist: " + sampleFile.getFullPathName();
        return result;
    }
    
    if (!sampleFile.hasReadAccess())
    {
        result.errorMessage = "Cannot read file: " + sampleFile.getFileName() + " (check permissions)";
        return result;
    }
    
    if (sampleFile.getSize() == 0)
    {
        result.errorMessage = "File is empty: " + sampleFile.getFileName();
        return result;
    }
    
    if (sampleFile.getSize() > 500 * 1024 * 1024) // 500MB limit
    {
        result.errorMessage = "File too large: " + sampleFile.getFileName() + " (max 500MB)";
        return result;
    }
    
    // Attempt to create audio reader
    auto reader = std::unique_ptr<juce::AudioFormatReader>(formatManager.createReaderFor(sampleFile));
    
    if (reader == nullptr)
    {
        juce::String extension = sampleFile.getFileExtension().toLowerCase();
        result.errorMessage = "Unsupported audio format: " + extension + 
                             "\nSupported: WAV, AIFF, MP3, FLAC, OGG";
        return result;
    }
    
    // Validate audio properties
    if (reader->lengthInSamples <= 0)
    {
        result.errorMessage = "Invalid audio file: " + sampleFile.getFileName() + " (no audio data)";
        return result;
    }
    
    if (reader->numChannels < 1 || reader->numChannels > 8)
    {
        result.errorMessage = "Unsupported channel count: " + juce::String(reader->numChannels) + 
                             " (supported: 1-8 channels)";
        return result;
    }
    
    if (reader->sampleRate < 8000 || reader->sampleRate > 192000)
    {
        result.errorMessage = "Unsupported sample rate: " + juce::String(reader->sampleRate) + 
                             "Hz (supported: 8kHz-192kHz)";
        return result;
    }
    
    try
    {
        // Create buffer for audio data
        auto newBuffer = std::make_unique<juce::AudioBuffer<float>>(
            static_cast<int>(reader->numChannels),
            static_cast<int>(reader->lengthInSamples)
        );
        
        // Read audio data
        if (!reader->read(newBuffer.get(), 0, static_cast<int>(reader->lengthInSamples), 0, true, true))
        {
            result.errorMessage = "Failed to read audio data from: " + sampleFile.getFileName();
            return result;
        }
        
        // Load into engine
        loadSample(*newBuffer, reader->sampleRate);
        currentSampleName = sampleFile.getFileNameWithoutExtension();
        
        // Return success with metadata
        result.success = true;
        result.lengthSeconds = reader->lengthInSamples / reader->sampleRate;
        result.sampleRate = static_cast<int>(reader->sampleRate);
        result.channels = static_cast<int>(reader->numChannels);
        
        return result;
    }
    catch (const std::exception& e)
    {
        result.errorMessage = "Error loading " + sampleFile.getFileName() + ": " + juce::String(e.what());
        return result;
    }
    catch (...)
    {
        result.errorMessage = "Unknown error loading: " + sampleFile.getFileName();
        return result;
    }
}

void SampleMaskingEngine::loadSample(const juce::AudioBuffer<float>& sampleBuffer_, double sourceSampleRate_)
{
    sampleBuffer = std::make_unique<juce::AudioBuffer<float>>(sampleBuffer_);
    sourceSampleRate = sourceSampleRate_;
    currentSampleName = "Loaded Sample";
    
    // Reset playback state
    playbackPosition.store(0.0);
    isPlaying.store(false);
    
    // Adjust time range based on sample length
    const double lengthSeconds = sampleBuffer->getNumSamples() / sourceSampleRate;
    setTimeRange(0.0f, static_cast<float>(lengthSeconds));
}

void SampleMaskingEngine::clearSample()
{
    stopPlayback();
    sampleBuffer.reset();
    currentSampleName.clear();
    clearAllMasks();
}

double SampleMaskingEngine::getSampleLengthSeconds() const
{
    if (!hasSample()) return 0.0;
    return sampleBuffer->getNumSamples() / sourceSampleRate;
}

//==============================================================================
// Sample Playback Control

void SampleMaskingEngine::startPlayback()
{
    if (hasSample())
    {
        isPlaying.store(true);
    }
}

void SampleMaskingEngine::stopPlayback()
{
    isPlaying.store(false);
    playbackPosition.store(0.0);
}

void SampleMaskingEngine::pausePlayback()
{
    isPlaying.store(false);
}

void SampleMaskingEngine::setPlaybackPosition(float normalizedPosition)
{
    if (!hasSample()) return;
    
    const double sampleLength = static_cast<double>(sampleBuffer->getNumSamples());
    const double newPosition = juce::jlimit(0.0, sampleLength, normalizedPosition * sampleLength);
    playbackPosition.store(newPosition);
}

//==============================================================================
// Paint Mask Management

juce::uint32 SampleMaskingEngine::createPaintMask(MaskingMode mode, juce::Colour color)
{
    juce::ScopedLock lock(maskLock);
    
    // Prevent mask ID overflow and DoS attacks
    const size_t MAX_ACTIVE_MASKS = 10000; // Reasonable limit
    if (activeMasks.size() >= MAX_ACTIVE_MASKS)
    {
        // RELIABILITY FIX: Remove jassert from potential audio thread context
        // Log error without blocking audio processing
        DBG("SampleMaskingEngine: Too many active masks (" << activeMasks.size() << "), dropping new mask");
        return 0; // Return invalid mask ID
    }
    
    // Prevent mask ID overflow (wrap around to 1, never use 0)
    if (nextMaskId == 0 || nextMaskId >= std::numeric_limits<juce::uint32>::max() - 1)
    {
        nextMaskId = 1; // Reset to start, avoiding 0 (invalid ID)
    }
    
    PaintMask newMask;
    newMask.maskId = nextMaskId++;
    newMask.mode = mode;
    newMask.maskColor = color;
    
    // Set default parameters based on mode
    switch (mode)
    {
        case MaskingMode::Volume:
            newMask.param1 = 0.0f; newMask.param2 = 1.0f;
            break;
        case MaskingMode::Filter:
            newMask.param1 = 100.0f; newMask.param2 = 8000.0f; newMask.param3 = 0.0f;
            break;
        case MaskingMode::Pitch:
            newMask.param1 = -12.0f; newMask.param2 = 12.0f;
            break;
        case MaskingMode::Granular:
            newMask.param1 = 0.1f; newMask.param2 = 0.5f;
            break;
        case MaskingMode::Chop:
            newMask.param1 = 16.0f; newMask.param2 = 1.0f;
            break;
        case MaskingMode::Stutter:
            newMask.param1 = 8.0f; newMask.param2 = 0.125f;
            break;
        case MaskingMode::Ring:
            newMask.param1 = 40.0f; newMask.param2 = 0.5f;
            break;
        case MaskingMode::Distortion:
            newMask.param1 = 1.0f; newMask.param2 = 0.5f;
            break;
        case MaskingMode::Delay:
            newMask.param1 = 0.25f; newMask.param2 = 0.3f; newMask.param3 = 0.3f;
            break;
        default:
            break;
    }
    
    activeMasks.push_back(newMask);
    rebuildActiveMasksSnapshotLocked();
    return newMask.maskId;
}

void SampleMaskingEngine::addPointToMask(juce::uint32 maskId, float x, float y, float pressure)
{
    // Input validation - critical security measure
    if (maskId == 0)
    {
        // RELIABILITY FIX: Silent validation without blocking audio thread
        DBG("SampleMaskingEngine: Invalid mask ID (0) in addPointToMask");
        return;
    }
    
    // Validate coordinate bounds (normalized 0.0-1.0 range expected)
    if (x < 0.0f || x > 1.0f || y < 0.0f || y > 1.0f)
    {
        // RELIABILITY FIX: Clamp coordinates instead of blocking
        x = juce::jlimit(0.0f, 1.0f, x);
        y = juce::jlimit(0.0f, 1.0f, y);
        DBG("SampleMaskingEngine: Clamped out-of-bounds coordinates");
    }
    
    // Validate pressure range
    if (pressure < 0.0f || pressure > 1.0f)
    {
        // RELIABILITY FIX: Clamp pressure instead of blocking
        pressure = juce::jlimit(0.0f, 1.0f, pressure);
        DBG("SampleMaskingEngine: Clamped out-of-bounds pressure");
    }
    
    // Check for NaN or infinite values
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(pressure))
    {
        // RELIABILITY FIX: Replace invalid values with safe defaults
        x = std::isfinite(x) ? x : 0.5f;
        y = std::isfinite(y) ? y : 0.5f;
        pressure = std::isfinite(pressure) ? pressure : 1.0f;
        DBG("SampleMaskingEngine: Replaced invalid floating point values with defaults");
    }
    
    juce::ScopedLock lock(maskLock);
    
    for (auto& mask : activeMasks)
    {
        if (mask.maskId == maskId)
        {
            mask.paintPath.lineTo(x, y);
            break;
        }
    }
    rebuildActiveMasksSnapshotLocked();
}

void SampleMaskingEngine::finalizeMask(juce::uint32 maskId)
{
    juce::ScopedLock lock(maskLock);
    
    for (auto& mask : activeMasks)
    {
        if (mask.maskId == maskId)
        {
            mask.isActive = true;
            break;
        }
    }
    rebuildActiveMasksSnapshotLocked();
}

void SampleMaskingEngine::removeMask(juce::uint32 maskId)
{
    juce::ScopedLock lock(maskLock);
    
    activeMasks.erase(
        std::remove_if(activeMasks.begin(), activeMasks.end(),
            [maskId](const PaintMask& mask) { return mask.maskId == maskId; }),
        activeMasks.end()
    );
    rebuildActiveMasksSnapshotLocked();
}

void SampleMaskingEngine::clearAllMasks()
{
    juce::ScopedLock lock(maskLock);
    activeMasks.clear();
    rebuildActiveMasksSnapshotLocked();
}

void SampleMaskingEngine::setMaskMode(juce::uint32 maskId, MaskingMode mode)
{
    juce::ScopedLock lock(maskLock);
    
    for (auto& mask : activeMasks)
    {
        if (mask.maskId == maskId)
        {
            mask.mode = mode;
            
            // Reset parameters to defaults for the new mode
            switch (mode)
            {
                case MaskingMode::Volume:
                    mask.param1 = 0.0f; mask.param2 = 1.0f;
                    break;
                case MaskingMode::Filter:
                    mask.param1 = 100.0f; mask.param2 = 8000.0f; mask.param3 = 0.0f;
                    break;
                case MaskingMode::Pitch:
                    mask.param1 = -12.0f; mask.param2 = 12.0f;
                    break;
                case MaskingMode::Granular:
                    mask.param1 = 0.1f; mask.param2 = 0.5f;
                    break;
                case MaskingMode::Chop:
                    mask.param1 = 16.0f; mask.param2 = 1.0f;
                    break;
                case MaskingMode::Stutter:
                    mask.param1 = 8.0f; mask.param2 = 0.125f;
                    break;
                case MaskingMode::Ring:
                    mask.param1 = 40.0f; mask.param2 = 0.5f;
                    break;
                case MaskingMode::Distortion:
                    mask.param1 = 1.0f; mask.param2 = 0.5f;
                    break;
                case MaskingMode::Delay:
                    mask.param1 = 0.25f; mask.param2 = 0.3f; mask.param3 = 0.3f;
                    break;
                default:
                    break;
            }
            break;
        }
    }
    rebuildActiveMasksSnapshotLocked();
}

void SampleMaskingEngine::setMaskIntensity(juce::uint32 maskId, float intensity)
{
    juce::ScopedLock lock(maskLock);
    
    for (auto& mask : activeMasks)
    {
        if (mask.maskId == maskId)
        {
            mask.intensity = juce::jlimit(0.0f, 1.0f, intensity);
            break;
        }
    }
    rebuildActiveMasksSnapshotLocked();
}

void SampleMaskingEngine::setMaskParameters(juce::uint32 maskId, float param1, float param2, float param3)
{
    // Input validation for security
    if (maskId == 0)
    {
        // RELIABILITY FIX: Silent validation without blocking audio thread
        DBG("SampleMaskingEngine: Invalid mask ID (0) in setMaskParameters");
        return;
    }
    
    // Check for NaN or infinite values in parameters
    if (!std::isfinite(param1) || !std::isfinite(param2) || !std::isfinite(param3))
    {
        // RELIABILITY FIX: Replace invalid values with safe defaults
        param1 = std::isfinite(param1) ? param1 : 0.0f;
        param2 = std::isfinite(param2) ? param2 : 1.0f;
        param3 = std::isfinite(param3) ? param3 : 0.0f;
        DBG("SampleMaskingEngine: Replaced invalid parameter values with defaults");
    }
    
    // Reasonable bounds checking for parameters (prevent extreme values)
    const float MAX_PARAM_VALUE = 1000000.0f; // Reasonable upper bound
    const float MIN_PARAM_VALUE = -1000000.0f; // Reasonable lower bound
    
    if (param1 < MIN_PARAM_VALUE || param1 > MAX_PARAM_VALUE ||
        param2 < MIN_PARAM_VALUE || param2 > MAX_PARAM_VALUE ||
        param3 < MIN_PARAM_VALUE || param3 > MAX_PARAM_VALUE)
    {
        // RELIABILITY FIX: Clamp parameters instead of blocking
        param1 = juce::jlimit(MIN_PARAM_VALUE, MAX_PARAM_VALUE, param1);
        param2 = juce::jlimit(MIN_PARAM_VALUE, MAX_PARAM_VALUE, param2);
        param3 = juce::jlimit(MIN_PARAM_VALUE, MAX_PARAM_VALUE, param3);
        DBG("SampleMaskingEngine: Clamped parameter values to reasonable bounds");
    }
    
    juce::ScopedLock lock(maskLock);
    
    for (auto& mask : activeMasks)
    {
        if (mask.maskId == maskId)
        {
            mask.param1 = param1;
            mask.param2 = param2;
            mask.param3 = param3;
            break;
        }
    }
    rebuildActiveMasksSnapshotLocked();
}

//==============================================================================
// Snapshot rebuild (UI thread only, must be called under maskLock)
void SampleMaskingEngine::rebuildActiveMasksSnapshotLocked()
{
    // Create a new snapshot copy of activeMasks and publish atomically (C++17 portable)
    auto copy = std::make_shared<const std::vector<PaintMask>>(activeMasks.begin(), activeMasks.end());
    std::atomic_store_explicit(&activeMasksSnapshot, copy, std::memory_order_release);
}

//==============================================================================
// Real-Time Paint Interface

void SampleMaskingEngine::beginPaintStroke(float x, float y, MaskingMode mode)
{
    auto maskId = createPaintMask(mode);
    currentPaintMask = std::make_unique<PaintMask>();
    currentPaintMask->maskId = maskId;
    currentPaintMask->mode = mode;
    currentPaintMask->paintPath.startNewSubPath(x, y);
}

void SampleMaskingEngine::updatePaintStroke(float x, float y, float pressure)
{
    if (currentPaintMask)
    {
        currentPaintMask->paintPath.lineTo(x, y);
        // Update the active mask in real-time
        addPointToMask(currentPaintMask->maskId, x, y, pressure);
    }
}

void SampleMaskingEngine::endPaintStroke()
{
    if (currentPaintMask)
    {
        finalizeMask(currentPaintMask->maskId);
        currentPaintMask.reset();
    }
}

//==============================================================================
// Canvas Coordinate System

void SampleMaskingEngine::setCanvasSize(float width, float height)
{
    canvasWidth = width;
    canvasHeight = height;
}

void SampleMaskingEngine::setTimeRange(float startSeconds, float endSeconds)
{
    timeRangeStart = startSeconds;
    timeRangeEnd = endSeconds;
}

float SampleMaskingEngine::canvasXToSampleTime(float x) const
{
    return timeRangeStart + (x / canvasWidth) * (timeRangeEnd - timeRangeStart);
}

float SampleMaskingEngine::sampleTimeToCanvasX(float timeSeconds) const
{
    return ((timeSeconds - timeRangeStart) / (timeRangeEnd - timeRangeStart)) * canvasWidth;
}

//==============================================================================
// Mask Application Methods

float SampleMaskingEngine::calculateMaskInfluence(const PaintMask& mask, double currentTimeSeconds) const
{
    const float canvasX = sampleTimeToCanvasX(static_cast<float>(currentTimeSeconds));
    
    // Check if current time intersects with mask path
    auto bounds = mask.paintPath.getBounds();
    if (canvasX < bounds.getX() || canvasX > bounds.getRight())
        return 0.0f;
    
    // Simple vertical intersection test
    // In a real implementation, this would be more sophisticated
    const float normalizedY = (canvasHeight * 0.5f - bounds.getCentreY()) / canvasHeight + 0.5f;
    return juce::jlimit(0.0f, 1.0f, normalizedY) * mask.intensity;
}

float SampleMaskingEngine::applyVolumeMask(const PaintMask& mask, float input, double timeSeconds)
{
    const float influence = calculateMaskInfluence(mask, timeSeconds);
    if (influence <= 0.0f) return input;
    
    const float minLevel = mask.param1;
    const float maxLevel = mask.param2;
    const float targetVolume = minLevel + influence * (maxLevel - minLevel);
    
    return input * targetVolume;
}

float SampleMaskingEngine::applyFilterMask(const PaintMask& mask, float input, double timeSeconds)
{
    const float influence = calculateMaskInfluence(mask, timeSeconds);
    if (influence <= 0.0f) return input;
    
    const float minCutoff = mask.param1;
    const float maxCutoff = mask.param2;
    const float targetCutoff = minCutoff + influence * (maxCutoff - minCutoff);
    
    maskFilter.setParams(targetCutoff, mask.param3, currentSampleRate);
    return maskFilter.process(input);
}

float SampleMaskingEngine::applyPitchMask(const PaintMask& mask, float input, double timeSeconds)
{
    const float influence = calculateMaskInfluence(mask, timeSeconds);
    if (influence <= 0.0f) return input;
    
    // Simple pitch shifting placeholder
    // Real implementation would use phase vocoder or granular synthesis
    const float minSemitones = mask.param1;
    const float maxSemitones = mask.param2;
    const float pitchShift = minSemitones + influence * (maxSemitones - minSemitones);
    
    // For now, just apply gain change (crude pitch effect)
    const float pitchGain = std::pow(2.0f, pitchShift / 12.0f);
    return input * pitchGain;
}

float SampleMaskingEngine::applyGranularMask(const PaintMask& mask, float input, double timeSeconds)
{
    const float influence = calculateMaskInfluence(mask, timeSeconds);
    if (influence <= 0.0f) return input;
    
    // Granular processing with mask influence
    const double samplePos = timeSeconds * currentSampleRate;
    return granularProcessor.processGrains(*sampleBuffer, samplePos) * influence + input * (1.0f - influence);
}

float SampleMaskingEngine::applyChopMask(const PaintMask& mask, float input, double timeSeconds)
{
    const float influence = calculateMaskInfluence(mask, timeSeconds);
    if (influence <= 0.0f) return input;
    
    const float chopRate = mask.param1;
    const float chopPhase = std::fmod(static_cast<float>(timeSeconds * chopRate), 1.0f);
    const bool isChopped = chopPhase < 0.5f;
    
    return isChopped ? input * mask.param2 * influence : input;
}

float SampleMaskingEngine::applyStutterMask(const PaintMask& mask, float input, double timeSeconds)
{
    const float influence = calculateMaskInfluence(mask, timeSeconds);
    if (influence <= 0.0f) return input;
    
    const float stutterRate = mask.param1;
    const float stutterLength = mask.param2;
    const float phase = std::fmod(static_cast<float>(timeSeconds * stutterRate), 1.0f);
    
    return (phase < stutterLength) ? input * influence : input * (1.0f - influence);
}

float SampleMaskingEngine::applyRingMask(const PaintMask& mask, float input, double timeSeconds)
{
    const float influence = calculateMaskInfluence(mask, timeSeconds);
    if (influence <= 0.0f) return input;
    
    const float frequency = mask.param1;
    const float depth = mask.param2;
    const float ringMod = std::sin(static_cast<float>(timeSeconds * frequency * 2.0 * juce::MathConstants<float>::pi));
    
    return input * (1.0f + ringMod * depth * influence);
}

float SampleMaskingEngine::applyDistortionMask(const PaintMask& mask, float input, double timeSeconds)
{
    const float influence = calculateMaskInfluence(mask, timeSeconds);
    if (influence <= 0.0f) return input;
    
    const float drive = mask.param1;
    const float mix = mask.param2;
    
    const float driven = std::tanh(input * drive * influence);
    return input * (1.0f - mix * influence) + driven * mix * influence;
}

float SampleMaskingEngine::applyDelayMask(const PaintMask& mask, float input, double timeSeconds)
{
    const float influence = calculateMaskInfluence(mask, timeSeconds);
    if (influence <= 0.0f) return input;
    
    const float delayTime = mask.param1;
    const float feedback = mask.param2;
    const float mix = mask.param3;
    
    delayLine.write(input);
    const float delayed = delayLine.readInterpolated(delayTime * static_cast<float>(currentSampleRate));
    delayLine.write(input + delayed * feedback * influence);
    
    return input * (1.0f - mix * influence) + delayed * mix * influence;
}

//==============================================================================
// Layer Management

void SampleMaskingEngine::createLayer(const juce::String& layerName)
{
    MaskLayer newLayer;
    newLayer.name = layerName;
    maskLayers.push_back(newLayer);
}

void SampleMaskingEngine::setActiveLayer(const juce::String& layerName)
{
    for (size_t i = 0; i < maskLayers.size(); ++i)
    {
        if (maskLayers[i].name == layerName)
        {
            activeMaskLayer = static_cast<int>(i);
            break;
        }
    }
}

void SampleMaskingEngine::removeLayer(const juce::String& layerName)
{
    maskLayers.erase(
        std::remove_if(maskLayers.begin(), maskLayers.end(),
            [&layerName](const MaskLayer& layer) { return layer.name == layerName; }),
        maskLayers.end()
    );
}

juce::StringArray SampleMaskingEngine::getLayerNames() const
{
    juce::StringArray names;
    for (const auto& layer : maskLayers)
    {
        names.add(layer.name);
    }
    return names;
}

//==============================================================================
// Helper Classes Implementation

void SampleMaskingEngine::MaskFilter::setParams(float newCutoff, float newResonance, double sampleRate)
{
    cutoff = newCutoff;
    resonance = newResonance;
    
    f = 2.0f * std::sin(static_cast<float>(juce::MathConstants<float>::pi) * cutoff / static_cast<float>(sampleRate));
    fb = resonance + resonance / (1.0f - f);
}

float SampleMaskingEngine::MaskFilter::process(float input)
{
    low += f * band;
    high = input - low - fb * band;
    band += f * high;
    
    return low; // Low-pass output
}

void SampleMaskingEngine::DelayLine::setMaxDelay(double maxDelaySeconds, double sampleRate)
{
    maxDelayInSamples = static_cast<int>(maxDelaySeconds * sampleRate);
    buffer.resize(maxDelayInSamples);
    std::fill(buffer.begin(), buffer.end(), 0.0f);
    writePos = 0;
}

void SampleMaskingEngine::DelayLine::write(float input)
{
    buffer[writePos] = input;
    writePos = (writePos + 1) % maxDelayInSamples;
}

float SampleMaskingEngine::DelayLine::readInterpolated(float delayInSamples)
{
    const float readPos = static_cast<float>(writePos) - delayInSamples;
    const int index1 = static_cast<int>(readPos) % maxDelayInSamples;
    const int index2 = (index1 + 1) % maxDelayInSamples;
    const float fraction = readPos - std::floor(readPos);
    
    const float sample1 = buffer[index1 < 0 ? index1 + maxDelayInSamples : index1];
    const float sample2 = buffer[index2 < 0 ? index2 + maxDelayInSamples : index2];
    
    return sample1 * (1.0f - fraction) + sample2 * fraction;
}

float SampleMaskingEngine::GranularProcessor::processGrains(const juce::AudioBuffer<float>& source, double currentPos)
{
    float output = 0.0f;
    
    for (auto& grain : grains)
    {
        if (grain.isActive)
        {
            const int sourceIndex = static_cast<int>(grain.position + grain.playbackPos);
            if (sourceIndex >= 0 && sourceIndex < source.getNumSamples())
            {
                const float sample = source.getSample(0, sourceIndex);
                output += sample * grain.envelope;
                
                grain.playbackPos += 1.0;
                grain.envelope *= 0.999f; // Simple decay
                
                if (grain.playbackPos >= grain.size)
                {
                    grain.isActive = false;
                }
            }
        }
    }
    
    return output;
}

//==============================================================================
// Tempo Synchronization Implementation (NEW for Beatmakers!)

void SampleMaskingEngine::setHostTempo(double bpm)
{
    hostTempo.store(juce::jlimit(60.0, 200.0, bpm));
    updateTimeStretchRatio();
}

void SampleMaskingEngine::setHostTimeSignature(int numerator, int denominator)
{
    hostTimeSignatureNumerator = juce::jlimit(1, 16, numerator);
    hostTimeSignatureDenominator = juce::jlimit(1, 16, denominator);
}

void SampleMaskingEngine::setHostPosition(double ppqPosition, bool isPlaying)
{
    hostPPQPosition.store(ppqPosition);
    hostIsPlaying.store(isPlaying);
}

SampleMaskingEngine::TempoInfo SampleMaskingEngine::detectSampleTempo()
{
    if (!hasSample())
    {
        return TempoInfo{};
    }
    
    // Analyze the sample for tempo
    spectralAnalyzer->analyzeBuffer(*sampleBuffer, sourceSampleRate);
    currentTempoInfo = spectralAnalyzer->getTempoInfo();
    
    return currentTempoInfo;
}

void SampleMaskingEngine::setSampleTempo(double bpm)
{
    sampleTempo.store(juce::jlimit(30.0, 300.0, bpm)); // Wider range for varied samples
    currentTempoInfo.detectedBPM = bpm;
    
    updateTimeStretchRatio();
}

void SampleMaskingEngine::updateTimeStretchRatio()
{
    if (tempoSyncEnabled.load())
    {
        const double host = hostTempo.load();
        const double sample = sampleTempo.load();
        
        // Avoid division by zero
        if (sample > 0.0)
        {
            timeStretchRatio = host / sample;
        }
        else
        {
            timeStretchRatio = 1.0;
        }
    }
    else
    {
        timeStretchRatio = 1.0; // No stretching when sync disabled
    }
}

void SampleMaskingEngine::setQuantization(QuantizeGrid grid)
{
    quantizeGrid.store(grid);
}

void SampleMaskingEngine::setQuantizationStrength(float strength)
{
    quantizationStrength.store(juce::jlimit(0.0f, 1.0f, strength));
}

void SampleMaskingEngine::setSwingAmount(float swing)
{
    swingAmount.store(juce::jlimit(0.0f, 1.0f, swing));
}

float SampleMaskingEngine::quantizeTime(float timeSeconds) const
{
    const auto grid = quantizeGrid.load();
    if (grid == QuantizeGrid::Off)
        return timeSeconds;
    
    const float strength = quantizationStrength.load();
    if (strength <= 0.0f)
        return timeSeconds;
    
    return calculateQuantizedBeatTime(timeSeconds);
}

double SampleMaskingEngine::calculateQuantizedBeatTime(double timeSeconds) const
{
    const auto grid = quantizeGrid.load();
    
    // Handle off case first
    if (grid == QuantizeGrid::Off)
    {
        return timeSeconds; // No quantization
    }
    
    const double tempo = hostTempo.load();
    const double beatsPerSecond = tempo / 60.0;
    const double secondsPerBeat = 1.0 / beatsPerSecond;
    
    double gridSize;
    
    // Handle special dotted cases
    if (grid == QuantizeGrid::DottedQuarter)
    {
        gridSize = secondsPerBeat * 1.5; // Dotted quarter = 1.5 beats
    }
    else if (grid == QuantizeGrid::DottedEighth)
    {
        gridSize = (secondsPerBeat / 2.0) * 1.5; // Dotted eighth = 1.5 eighth notes
    }
    else
    {
        // Standard grid subdivision
        gridSize = secondsPerBeat / static_cast<double>(grid);
    }
    
    // Quantize to grid
    const double gridPosition = timeSeconds / gridSize;
    const double quantizedGrid = std::round(gridPosition);
    const double quantizedTime = quantizedGrid * gridSize;
    
    // Apply swing if enabled (only for standard grids, not dotted)
    const float swing = swingAmount.load();
    if (swing > 0.0f && 
        grid != QuantizeGrid::DottedQuarter && 
        grid != QuantizeGrid::DottedEighth &&
        static_cast<int>(quantizedGrid) % 2 == 1)
    {
        // More musical swing calculation using exponential curve
        const double swingOffset = gridSize * swing * 0.3 * (1.0 - std::exp(-swing * 2.0));
        return quantizedTime + swingOffset;
    }
    
    // Blend between original and quantized based on strength
    const float strength = quantizationStrength.load();
    return timeSeconds * (1.0 - strength) + quantizedTime * strength;
}

//==============================================================================
// Spectral Analyzer Implementation

void SampleMaskingEngine::SpectralAnalyzer::analyzeBuffer(const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    if (buffer.getNumSamples() == 0 || buffer.getNumChannels() == 0)
        return;
    
    // Initialize FFT if needed
    if (!fft)
    {
        fft = std::make_unique<juce::dsp::FFT>(FFT_ORDER);
        fftData.resize(FFT_SIZE * 2);
        window.resize(FFT_SIZE);
        magnitudes.resize(FFT_SIZE / 2);
        
        // Create Hann window
        for (int i = 0; i < FFT_SIZE; ++i)
        {
            window[i] = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * i / (FFT_SIZE - 1)));
        }
    }
    
    // Process buffer in chunks for tempo detection
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    const int chunkSize = std::min(FFT_SIZE, numSamples);
    
    // Clear FFT data
    std::fill(fftData.begin(), fftData.end(), 0.0f);
    
    // Copy audio data (mono mix if stereo)
    for (int i = 0; i < chunkSize; ++i)
    {
        float sample = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            sample += buffer.getSample(ch, i);
        }
        
        fftData[i] = (sample / numChannels) * window[i];
    }
    
    // Perform FFT
    fft->performFrequencyOnlyForwardTransform(fftData.data());
    
    // Extract magnitudes
    for (int i = 0; i < FFT_SIZE / 2; ++i)
    {
        const float real = fftData[i * 2];
        const float imag = fftData[i * 2 + 1];
        magnitudes[i] = std::sqrt(real * real + imag * imag);
    }
    
    // Detect tempo from spectral data
    detectTempo(magnitudes, sampleRate);
    
    // Detect beats from time-domain data
    detectBeats(buffer, sampleRate);
}

void SampleMaskingEngine::SpectralAnalyzer::detectTempo(const std::vector<float>& spectrum, double sampleRate)
{
    // Enhanced tempo detection using multiple frequency ranges
    // This is still basic but more robust than single-peak detection
    
    // Define frequency ranges for different drum elements
    const int kickLowBin = static_cast<int>((40.0 / (sampleRate * 0.5)) * spectrum.size());
    const int kickHighBin = static_cast<int>((100.0 / (sampleRate * 0.5)) * spectrum.size());
    const int snareLowBin = static_cast<int>((150.0 / (sampleRate * 0.5)) * spectrum.size());
    const int snareHighBin = static_cast<int>((300.0 / (sampleRate * 0.5)) * spectrum.size());
    const int hihatLowBin = static_cast<int>((5000.0 / (sampleRate * 0.5)) * spectrum.size());
    const int hihatHighBin = static_cast<int>((12000.0 / (sampleRate * 0.5)) * spectrum.size());
    
    // Calculate energy in each frequency band
    float kickEnergy = 0.0f;
    float snareEnergy = 0.0f;
    float hihatEnergy = 0.0f;
    
    // Kick drum energy (40-100 Hz)
    for (int i = kickLowBin; i < kickHighBin && i < spectrum.size(); ++i)
    {
        kickEnergy += spectrum[i] * spectrum[i];
    }
    kickEnergy = std::sqrt(kickEnergy / (kickHighBin - kickLowBin));
    
    // Snare energy (150-300 Hz)
    for (int i = snareLowBin; i < snareHighBin && i < spectrum.size(); ++i)
    {
        snareEnergy += spectrum[i] * spectrum[i];
    }
    snareEnergy = std::sqrt(snareEnergy / (snareHighBin - snareLowBin));
    
    // Hi-hat energy (5-12 kHz)
    for (int i = hihatLowBin; i < hihatHighBin && i < spectrum.size(); ++i)
    {
        hihatEnergy += spectrum[i] * spectrum[i];
    }
    hihatEnergy = std::sqrt(hihatEnergy / (hihatHighBin - hihatLowBin));
    
    // Calculate total rhythmic energy
    const float totalRhythmicEnergy = kickEnergy + snareEnergy + hihatEnergy;
    
    // Estimate tempo based on rhythmic content
    if (totalRhythmicEnergy > 0.05f)
    {
        // Determine predominant element and estimate BPM accordingly
        if (kickEnergy > snareEnergy && kickEnergy > hihatEnergy)
        {
            // Kick-heavy: likely slower tempo (80-140 BPM)
            tempoInfo.detectedBPM = 80.0 + (kickEnergy * 60.0);
        }
        else if (hihatEnergy > kickEnergy && hihatEnergy > snareEnergy)
        {
            // Hi-hat heavy: likely faster tempo (120-180 BPM)
            tempoInfo.detectedBPM = 120.0 + (hihatEnergy * 60.0);
        }
        else
        {
            // Balanced or snare-heavy: medium tempo (100-160 BPM)
            tempoInfo.detectedBPM = 100.0 + (totalRhythmicEnergy * 60.0);
        }
        
        // Clamp to reasonable BPM range
        tempoInfo.detectedBPM = juce::jlimit(60.0, 200.0, tempoInfo.detectedBPM);
        
        // Calculate confidence based on rhythmic content distribution
        const float maxEnergy = std::max({kickEnergy, snareEnergy, hihatEnergy});
        tempoInfo.confidence = juce::jlimit(0.0f, 1.0f, maxEnergy * 2.0f);
        tempoInfo.isTempoStable = tempoInfo.confidence > 0.6f;
    }
    else
    {
        // Low rhythmic content - might be a sustained sound or ambient texture
        tempoInfo.detectedBPM = 120.0; // Default
        tempoInfo.confidence = 0.1f;   // Low confidence
        tempoInfo.isTempoStable = false;
    }
}

void SampleMaskingEngine::SpectralAnalyzer::detectBeats(const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    // Simple beat detection using onset detection
    // This would be enhanced with proper onset detection algorithms
    
    const int numSamples = buffer.getNumSamples();
    const double lengthSeconds = numSamples / sampleRate;
    const double beatsPerSecond = tempoInfo.detectedBPM / 60.0;
    const double secondsPerBeat = 1.0 / beatsPerSecond;
    
    // Estimate first 4 beat positions
    for (int i = 0; i < 4; ++i)
    {
        tempoInfo.beatPositions[i] = i * secondsPerBeat;
        
        // Clamp to sample length
        if (tempoInfo.beatPositions[i] >= lengthSeconds)
        {
            tempoInfo.beatPositions[i] = lengthSeconds - 0.001;
        }
    }
}
