#include "LinearTrackerEngine.h"
#include <cmath>
#include <algorithm>

//==============================================================================
LinearTrackerEngine::LinearTrackerEngine()
{
    formatManager.registerBasicFormats();
    
    // Initialize default frequency ranges for linear drumming
    autoArrangeFrequencies();
    
    // Setup default canvas
    setCanvasSize(1000.0f, 600.0f);
    
    // Initialize pattern 0
    patterns[0].clear();
    patterns[0].name = "Pattern 01";
    
    // Setup default timing
    calculateTiming();
}

LinearTrackerEngine::~LinearTrackerEngine()
{
    releaseResources();
}

//==============================================================================
// Audio Processing

void LinearTrackerEngine::prepareToPlay(double sampleRate_, int samplesPerBlock_, int numChannels)
{
    sampleRate = sampleRate_;
    samplesPerBlock = samplesPerBlock_;
    
    // Reset all voices
    for (auto& voice : voices)
    {
        voice.isActive = false;
    }
    
    calculateTiming();
}

void LinearTrackerEngine::processBlock(juce::AudioBuffer<float>& buffer)
{
    auto startTime = juce::Time::getMillisecondCounter();
    
    if (!isPlaybackActive.load())
    {
        buffer.clear();
        return;
    }
    
    const int numSamples = buffer.getNumSamples();
    buffer.clear();
    
    // Snapshot active voices and required instrument references once per block (lock-free render loop)
    struct VoiceSnapshot { int instrumentIndex; int trackIndex; bool isActive; };
    std::array<VoiceSnapshot, MAX_VOICES> voiceSnapshot{};
    {
        juce::ScopedLock lock(voiceLock);
        for (size_t i = 0; i < voices.size(); ++i)
        {
            voiceSnapshot[i].isActive = voices[i].isActive;
            voiceSnapshot[i].instrumentIndex = voices[i].instrumentIndex;
            voiceSnapshot[i].trackIndex = voices[i].trackIndex;
        }
    }

    // Process each sample
    for (int sample = 0; sample < numSamples; ++sample)
    {
        // Check if we need to trigger new notes
        if (samplePosition >= nextRowPosition)
        {
            processRowTriggers();
            advancePlayback(1);
        }
        else
        {
            samplePosition += 1.0;
        }

        // Mix all active voices using the snapshot (no locks inside hot loop)
        float mixedOutput = 0.0f;
        for (size_t i = 0; i < voices.size(); ++i)
        {
            if (!voiceSnapshot[i].isActive) continue;
            const int instrIdx = voiceSnapshot[i].instrumentIndex;
            if (instrIdx < 0 || instrIdx >= (int)instruments.size()) continue;
            const auto& instrument = instruments[instrIdx];
            float voiceSample = voices[i].renderNextSample(instrument);
            mixedOutput += voiceSample;
            // Optional: visual feedback could be accumulated into a buffer and published later
        }

        // Apply to all channels
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            buffer.setSample(channel, sample, mixedOutput * 0.5f); // Simple attenuation
        }
    }
    
    // Update performance metrics
    auto endTime = juce::Time::getMillisecondCounter();
    auto processingTime = endTime - startTime;
    cpuUsage.store(static_cast<float>(processingTime) / (numSamples / sampleRate * 1000.0f));
}

void LinearTrackerEngine::releaseResources()
{
    stopPlayback();
    
    for (auto& voice : voices)
    {
        voice.isActive = false;
    }
    
    for (auto& instrument : instruments)
    {
        instrument.sampleBuffer.reset();
    }
}

//==============================================================================
// Pattern Management

void LinearTrackerEngine::TrackerPattern::clear()
{
    for (int track = 0; track < MAX_TRACKS; ++track)
    {
        for (int row = 0; row < MAX_PATTERN_LENGTH; ++row)
        {
            cells[track][row].clear();
        }
    }
}

void LinearTrackerEngine::TrackerPattern::resizePattern(int newLength)
{
    length = juce::jlimit(1, MAX_PATTERN_LENGTH, newLength);
}

void LinearTrackerEngine::setCurrentPattern(int patternIndex)
{
    if (patternIndex >= 0 && patternIndex < MAX_PATTERNS)
    {
        currentPatternIndex.store(patternIndex);
    }
}

void LinearTrackerEngine::copyPattern(int sourceIndex, int destIndex)
{
    if (sourceIndex >= 0 && sourceIndex < MAX_PATTERNS &&
        destIndex >= 0 && destIndex < MAX_PATTERNS)
    {
        juce::ScopedLock lock(patternLock);
        patterns[destIndex] = patterns[sourceIndex];
        patterns[destIndex].name = "Pattern " + juce::String(destIndex + 1).paddedLeft('0', 2);
    }
}

void LinearTrackerEngine::clearPattern(int patternIndex)
{
    if (patternIndex >= 0 && patternIndex < MAX_PATTERNS)
    {
        juce::ScopedLock lock(patternLock);
        patterns[patternIndex].clear();
    }
}

void LinearTrackerEngine::clearTrack(int patternIndex, int trackIndex)
{
    if (patternIndex >= 0 && patternIndex < MAX_PATTERNS &&
        trackIndex >= 0 && trackIndex < MAX_TRACKS)
    {
        juce::ScopedLock lock(patternLock);
        auto& pattern = patterns[patternIndex];
        
        for (int row = 0; row < pattern.length; ++row)
        {
            pattern.cells[trackIndex][row].clear();
        }
    }
}

//==============================================================================
// Linear Drumming Frequency Assignment

void LinearTrackerEngine::setTrackFrequencyRange(int trackIndex, DrumType drumType)
{
    if (trackIndex < 0 || trackIndex >= MAX_TRACKS) return;
    
    FrequencyRange& range = trackFrequencyRanges[trackIndex];
    range.drumType = drumType;
    
    // Set frequency ranges based on linear drumming principles
    switch (drumType)
    {
        case DrumType::Kick:
            range.lowFreq = 20.0f;
            range.highFreq = 80.0f;
            range.centerFreq = 50.0f;
            range.trackColor = juce::Colours::red;
            range.trackName = "Kick";
            break;
            
        case DrumType::Snare:
            range.lowFreq = 150.0f;
            range.highFreq = 250.0f;
            range.centerFreq = 200.0f;
            range.trackColor = juce::Colours::orange;
            range.trackName = "Snare";
            break;
            
        case DrumType::ClosedHat:
            range.lowFreq = 8000.0f;
            range.highFreq = 15000.0f;
            range.centerFreq = 11500.0f;
            range.trackColor = juce::Colours::yellow;
            range.trackName = "Closed Hat";
            break;
            
        case DrumType::OpenHat:
            range.lowFreq = 6000.0f;
            range.highFreq = 12000.0f;
            range.centerFreq = 9000.0f;
            range.trackColor = juce::Colours::lightyellow;
            range.trackName = "Open Hat";
            break;
            
        case DrumType::Crash:
            range.lowFreq = 3000.0f;
            range.highFreq = 8000.0f;
            range.centerFreq = 5500.0f;
            range.trackColor = juce::Colours::lightblue;
            range.trackName = "Crash";
            break;
            
        case DrumType::Ride:
            range.lowFreq = 4000.0f;
            range.highFreq = 10000.0f;
            range.centerFreq = 7000.0f;
            range.trackColor = juce::Colours::cyan;
            range.trackName = "Ride";
            break;
            
        case DrumType::Tom1:
            range.lowFreq = 80.0f;
            range.highFreq = 120.0f;
            range.centerFreq = 100.0f;
            range.trackColor = juce::Colours::green;
            range.trackName = "Tom 1";
            break;
            
        case DrumType::Tom2:
            range.lowFreq = 60.0f;
            range.highFreq = 100.0f;
            range.centerFreq = 80.0f;
            range.trackColor = juce::Colours::darkgreen;
            range.trackName = "Tom 2";
            break;
            
        case DrumType::Tom3:
            range.lowFreq = 40.0f;
            range.highFreq = 80.0f;
            range.centerFreq = 60.0f;
            range.trackColor = juce::Colours::forestgreen;
            range.trackName = "Tom 3";
            break;
            
        case DrumType::Clap:
            range.lowFreq = 1000.0f;
            range.highFreq = 3000.0f;
            range.centerFreq = 2000.0f;
            range.trackColor = juce::Colours::pink;
            range.trackName = "Clap";
            break;
            
        case DrumType::Rim:
            range.lowFreq = 2000.0f;
            range.highFreq = 5000.0f;
            range.centerFreq = 3500.0f;
            range.trackColor = juce::Colours::hotpink;
            range.trackName = "Rim";
            break;
            
        case DrumType::Shaker:
            range.lowFreq = 10000.0f;
            range.highFreq = 16000.0f;
            range.centerFreq = 13000.0f;
            range.trackColor = juce::Colours::white;
            range.trackName = "Shaker";
            break;
            
        default:
            range.lowFreq = 100.0f;
            range.highFreq = 1000.0f;
            range.centerFreq = 550.0f;
            range.trackColor = juce::Colours::grey;
            range.trackName = "Percussion";
            break;
    }
}

void LinearTrackerEngine::setCustomFrequencyRange(int trackIndex, float lowHz, float highHz)
{
    if (trackIndex < 0 || trackIndex >= MAX_TRACKS) return;
    
    FrequencyRange& range = trackFrequencyRanges[trackIndex];
    range.lowFreq = lowHz;
    range.highFreq = highHz;
    range.centerFreq = (lowHz + highHz) * 0.5f;
    range.trackName = "Custom " + juce::String(trackIndex + 1);
}

LinearTrackerEngine::FrequencyRange LinearTrackerEngine::getTrackFrequencyRange(int trackIndex) const
{
    if (trackIndex >= 0 && trackIndex < MAX_TRACKS)
    {
        return trackFrequencyRanges[trackIndex];
    }
    return FrequencyRange{};
}

void LinearTrackerEngine::autoArrangeFrequencies()
{
    // Arrange tracks according to linear drumming principles
    // Lower frequencies at bottom, higher at top
    setTrackFrequencyRange(0, DrumType::Kick);
    setTrackFrequencyRange(1, DrumType::Tom3);
    setTrackFrequencyRange(2, DrumType::Tom2);
    setTrackFrequencyRange(3, DrumType::Tom1);
    setTrackFrequencyRange(4, DrumType::Snare);
    setTrackFrequencyRange(5, DrumType::Clap);
    setTrackFrequencyRange(6, DrumType::Rim);
    setTrackFrequencyRange(7, DrumType::Crash);
    setTrackFrequencyRange(8, DrumType::Ride);
    setTrackFrequencyRange(9, DrumType::OpenHat);
    setTrackFrequencyRange(10, DrumType::ClosedHat);
    setTrackFrequencyRange(11, DrumType::Shaker);
    setTrackFrequencyRange(12, DrumType::Percussion1);
    setTrackFrequencyRange(13, DrumType::Percussion2);
    setTrackFrequencyRange(14, DrumType::FX1);
    setTrackFrequencyRange(15, DrumType::FX2);
}

bool LinearTrackerEngine::FrequencyRange::doesOverlap(const FrequencyRange& other) const
{
    return !(highFreq < other.lowFreq || lowFreq > other.highFreq);
}

bool LinearTrackerEngine::checkForFrequencyConflicts() const
{
    for (int i = 0; i < MAX_TRACKS - 1; ++i)
    {
        for (int j = i + 1; j < MAX_TRACKS; ++j)
        {
            if (trackFrequencyRanges[i].doesOverlap(trackFrequencyRanges[j]))
            {
                return true;
            }
        }
    }
    return false;
}

void LinearTrackerEngine::resolveFrequencyConflicts()
{
    // Simple conflict resolution: spread overlapping ranges
    for (int i = 0; i < MAX_TRACKS - 1; ++i)
    {
        for (int j = i + 1; j < MAX_TRACKS; ++j)
        {
            if (trackFrequencyRanges[i].doesOverlap(trackFrequencyRanges[j]))
            {
                separateConflictingTracks(i, j);
            }
        }
    }
}

//==============================================================================
// Paint-to-Tracker Interface

void LinearTrackerEngine::beginPaintStroke(float x, float y, float pressure, juce::Colour color)
{
    currentPaintStroke = std::make_unique<PaintStroke>();
    currentPaintStroke->trackIndex = canvasYToTrack(y);
    currentPaintStroke->startRow = canvasXToRow(x);
    currentPaintStroke->points.push_back({x, y});
    currentPaintStroke->pressure = pressure;
    currentPaintStroke->color = color;
    currentPaintStroke->timestamp = juce::Time::getMillisecondCounter();
    
    // Determine drum type from track
    if (currentPaintStroke->trackIndex >= 0 && currentPaintStroke->trackIndex < MAX_TRACKS)
    {
        currentPaintStroke->drumType = trackFrequencyRanges[currentPaintStroke->trackIndex].drumType;
    }
}

void LinearTrackerEngine::updatePaintStroke(float x, float y, float pressure)
{
    if (currentPaintStroke)
    {
        currentPaintStroke->points.push_back({x, y});
        currentPaintStroke->pressure = std::max(currentPaintStroke->pressure, pressure);
        currentPaintStroke->endRow = canvasXToRow(x);
    }
}

void LinearTrackerEngine::endPaintStroke()
{
    if (currentPaintStroke)
    {
        // Convert paint stroke to tracker pattern
        convertPaintToPattern(*currentPaintStroke);
        
        // Store for visual feedback
        recentPaintStrokes.push_back(*currentPaintStroke);
        
        // Limit stored strokes for performance
        if (recentPaintStrokes.size() > 50)
        {
            recentPaintStrokes.erase(recentPaintStrokes.begin());
        }
        
        currentPaintStroke.reset();
    }
}

void LinearTrackerEngine::setCanvasSize(float width, float height)
{
    canvasWidth = width;
    canvasHeight = height;
}

int LinearTrackerEngine::canvasXToRow(float x) const
{
    const float normalizedX = x / canvasWidth;
    const int patternLength = getCurrentPattern().length;
    return juce::jlimit(0, patternLength - 1, static_cast<int>(normalizedX * patternLength));
}

int LinearTrackerEngine::canvasYToTrack(float y) const
{
    const float normalizedY = 1.0f - (y / canvasHeight); // Invert Y (lower freq at bottom)
    return juce::jlimit(0, MAX_TRACKS - 1, static_cast<int>(normalizedY * MAX_TRACKS));
}

float LinearTrackerEngine::rowToCanvasX(int row) const
{
    const int patternLength = getCurrentPattern().length;
    return (static_cast<float>(row) / patternLength) * canvasWidth;
}

float LinearTrackerEngine::trackToCanvasY(int track) const
{
    const float normalizedY = 1.0f - (static_cast<float>(track) / MAX_TRACKS);
    return normalizedY * canvasHeight;
}

void LinearTrackerEngine::convertPaintToPattern(const PaintStroke& stroke)
{
    juce::ScopedLock lock(patternLock);
    
    auto& pattern = patterns[currentPatternIndex.load()];
    generateNotesFromStroke(stroke, pattern);
}

void LinearTrackerEngine::generateNotesFromStroke(const PaintStroke& stroke, TrackerPattern& pattern)
{
    if (stroke.trackIndex < 0 || stroke.trackIndex >= MAX_TRACKS) return;
    
    // Generate notes based on paint stroke characteristics
    const int startRow = juce::jmax(0, stroke.startRow);
    const int endRow = juce::jmin(pattern.length - 1, stroke.endRow);
    
    // Calculate note density based on stroke length and pressure
    const int strokeLength = endRow - startRow + 1;
    const float density = juce::jlimit(0.1f, 1.0f, stroke.pressure);
    const int noteCount = juce::jmax(1, static_cast<int>(strokeLength * density));
    
    // Generate notes at calculated intervals
    for (int i = 0; i < noteCount; ++i)
    {
        const int row = startRow + (i * strokeLength) / noteCount;
        if (row >= 0 && row < pattern.length)
        {
            TrackerCell& cell = pattern.cells[stroke.trackIndex][row];
            
            // Set note based on drum type
            cell.note = 60; // Middle C by default
            cell.instrument = stroke.trackIndex; // Use track index as instrument
            cell.volume = static_cast<int>(stroke.pressure * 64.0f);
            
            // Store paint-specific data
            cell.paintPressure = stroke.pressure;
            cell.paintColor = stroke.color;
            
            // Calculate velocity from stroke speed (if multiple points)
            if (stroke.points.size() > 1)
            {
                // Simple velocity calculation based on point spacing
                const float avgSpacing = stroke.points.back().getDistanceFrom(stroke.points.front()) / stroke.points.size();
                cell.paintVelocity = juce::jlimit(0.0f, 1.0f, avgSpacing / 10.0f);
            }
        }
    }
}

//==============================================================================
// Playback Control

void LinearTrackerEngine::startPlayback()
{
    isPlaybackActive.store(true);
    samplePosition = 0.0;
    nextRowPosition = 0.0;
}

void LinearTrackerEngine::stopPlayback()
{
    isPlaybackActive.store(false);
    currentRow.store(0);
    samplePosition = 0.0;
    
    // Stop all voices
    juce::ScopedLock lock(voiceLock);
    for (auto& voice : voices)
    {
        voice.stopNote();
    }
}

void LinearTrackerEngine::pausePlayback()
{
    isPlaybackActive.store(false);
}

void LinearTrackerEngine::setPlaybackPosition(int row)
{
    currentRow.store(juce::jlimit(0, getCurrentPattern().length - 1, row));
    samplePosition = currentRow.load() * samplesPerRow;
    nextRowPosition = samplePosition;
}

void LinearTrackerEngine::setTempo(float bpm)
{
    currentTempo.store(juce::jlimit(60.0f, 200.0f, bpm));
    calculateTiming();
}

void LinearTrackerEngine::calculateTiming()
{
    const float tempo = currentTempo.load();
    const double beatsPerSecond = tempo / 60.0;
    const double rowsPerSecond = beatsPerSecond * getCurrentPattern().rowsPerBeat;
    samplesPerRow = sampleRate / rowsPerSecond;
}

void LinearTrackerEngine::advancePlayback(int numSamples)
{
    const int row = currentRow.load();
    const int nextRow = (row + 1) % getCurrentPattern().length;
    
    currentRow.store(nextRow);
    nextRowPosition = (nextRow + 1) * samplesPerRow;
}

void LinearTrackerEngine::processRowTriggers()
{
    juce::ScopedLock lock(patternLock);
    
    const auto& pattern = getCurrentPattern();
    const int row = currentRow.load();
    
    if (row < 0 || row >= pattern.length) return;
    
    // Process all tracks for current row
    for (int track = 0; track < MAX_TRACKS; ++track)
    {
        const TrackerCell& cell = pattern.cells[track][row];
        
        if (!cell.isEmpty())
        {
            triggerNote(track, cell);
        }
    }
}

//==============================================================================
// Voice Management

LinearTrackerEngine::TrackerVoice* LinearTrackerEngine::findFreeVoice()
{
    for (auto& voice : voices)
    {
        if (!voice.isActive)
        {
            return &voice;
        }
    }
    
    // No free voice, steal oldest
    TrackerVoice* oldest = &voices[0];
    for (auto& voice : voices)
    {
        if (voice.envelopeLevel < oldest->envelopeLevel)
        {
            oldest = &voice;
        }
    }
    
    return oldest;
}

void LinearTrackerEngine::triggerNote(int trackIndex, const TrackerCell& cell)
{
    juce::ScopedLock lock(voiceLock);
    
    TrackerVoice* voice = findFreeVoice();
    if (voice && cell.instrument >= 0 && cell.instrument < MAX_INSTRUMENTS)
    {
        voice->startNote(trackIndex, cell.instrument, cell.note, cell.volume / 64.0f);
    }
}

void LinearTrackerEngine::TrackerVoice::startNote(int track, int instrument, int note, float vel)
{
    trackIndex = track;
    instrumentIndex = instrument;
    midiNote = note;
    volume = vel;
    samplePosition = 0.0;
    envelopeLevel = 0.0f;
    envStage = EnvStage::Attack;
    isActive = true;
    
    // Calculate pitch ratio
    const float semitoneOffset = midiNote - 60; // Middle C
    pitchRatio = std::pow(2.0f, semitoneOffset / 12.0f);
}

void LinearTrackerEngine::TrackerVoice::stopNote()
{
    if (envStage != EnvStage::Idle)
    {
        envStage = EnvStage::Release;
    }
}

float LinearTrackerEngine::TrackerVoice::processEnvelope(float attack, float decay, float sustain, float release)
{
    switch (envStage)
    {
        case EnvStage::Attack:
            envelopeLevel += 1.0f / (attack * 44100.0f);
            if (envelopeLevel >= 1.0f)
            {
                envelopeLevel = 1.0f;
                envStage = EnvStage::Decay;
            }
            break;
            
        case EnvStage::Decay:
            envelopeLevel -= (1.0f - sustain) / (decay * 44100.0f);
            if (envelopeLevel <= sustain)
            {
                envelopeLevel = sustain;
                envStage = EnvStage::Sustain;
            }
            break;
            
        case EnvStage::Sustain:
            envelopeLevel = sustain;
            break;
            
        case EnvStage::Release:
            envelopeLevel -= sustain / (release * 44100.0f);
            if (envelopeLevel <= 0.0f)
            {
                envelopeLevel = 0.0f;
                envStage = EnvStage::Idle;
                isActive = false;
            }
            break;
            
        case EnvStage::Idle:
            isActive = false;
            break;
    }
    
    return envelopeLevel;
}

float LinearTrackerEngine::TrackerVoice::renderNextSample(const TrackerInstrument& instrument)
{
    if (!isActive || !instrument.sampleBuffer) return 0.0f;
    
    // Get sample with interpolation
    const auto& buffer = *instrument.sampleBuffer;
    const int bufferLength = buffer.getNumSamples();
    
    if (samplePosition >= bufferLength)
    {
        isActive = false;
        return 0.0f;
    }
    
    // Linear interpolation
    const int index = static_cast<int>(samplePosition);
    const float fraction = static_cast<float>(samplePosition - index);
    
    float sample = 0.0f;
    if (index < bufferLength - 1)
    {
        const float sample1 = buffer.getSample(0, index);
        const float sample2 = buffer.getSample(0, index + 1);
        sample = sample1 * (1.0f - fraction) + sample2 * fraction;
    }
    else if (index < bufferLength)
    {
        sample = buffer.getSample(0, index);
    }
    
    // Apply envelope
    const float envLevel = processEnvelope(instrument.attack, instrument.decay, 
                                         instrument.sustain, instrument.release);
    
    // Advance sample position
    samplePosition += pitchRatio;
    
    return sample * envLevel * volume;
}

//==============================================================================
// Instrument Management

void LinearTrackerEngine::loadInstrument(int instrumentIndex, const juce::File& sampleFile)
{
    if (instrumentIndex < 0 || instrumentIndex >= MAX_INSTRUMENTS) return;
    
    auto reader = std::unique_ptr<juce::AudioFormatReader>(formatManager.createReaderFor(sampleFile));
    
    if (reader != nullptr)
    {
        auto& instrument = instruments[instrumentIndex];
        
        instrument.sampleBuffer = std::make_unique<juce::AudioBuffer<float>>(
            1, // Mono for now
            static_cast<int>(reader->lengthInSamples)
        );
        
        reader->read(instrument.sampleBuffer.get(), 0, static_cast<int>(reader->lengthInSamples), 0, true, false);
        
        instrument.sourceSampleRate = reader->sampleRate;
        instrument.name = sampleFile.getFileNameWithoutExtension();
        
        // Set frequency range based on track assignment
        // This would be enhanced with automatic frequency detection
    }
}

//==============================================================================
// Linear Drumming Analysis

bool LinearTrackerEngine::detectFrequencyMasking(int track1, int track2) const
{
    if (track1 < 0 || track1 >= MAX_TRACKS || track2 < 0 || track2 >= MAX_TRACKS)
        return false;
    
    return trackFrequencyRanges[track1].doesOverlap(trackFrequencyRanges[track2]);
}

float LinearTrackerEngine::calculateFrequencyOverlap(const FrequencyRange& range1, const FrequencyRange& range2) const
{
    if (!range1.doesOverlap(range2)) return 0.0f;
    
    const float overlapLow = std::max(range1.lowFreq, range2.lowFreq);
    const float overlapHigh = std::min(range1.highFreq, range2.highFreq);
    const float overlapSize = overlapHigh - overlapLow;
    
    const float range1Size = range1.highFreq - range1.lowFreq;
    const float range2Size = range2.highFreq - range2.lowFreq;
    const float avgRangeSize = (range1Size + range2Size) * 0.5f;
    
    return overlapSize / avgRangeSize;
}

void LinearTrackerEngine::separateConflictingTracks(int track1, int track2)
{
    if (track1 < 0 || track1 >= MAX_TRACKS || track2 < 0 || track2 >= MAX_TRACKS)
        return;
    
    auto& range1 = trackFrequencyRanges[track1];
    auto& range2 = trackFrequencyRanges[track2];
    
    // Simple separation: move higher track up, lower track down
    const float gap = 100.0f; // Hz separation
    
    if (range1.centerFreq < range2.centerFreq)
    {
        // range1 is lower, adjust range2 upward
        const float width = range2.highFreq - range2.lowFreq;
        range2.lowFreq = range1.highFreq + gap;
        range2.highFreq = range2.lowFreq + width;
        range2.centerFreq = (range2.lowFreq + range2.highFreq) * 0.5f;
    }
    else
    {
        // range2 is lower, adjust range1 upward
        const float width = range1.highFreq - range1.lowFreq;
        range1.lowFreq = range2.highFreq + gap;
        range1.highFreq = range1.lowFreq + width;
        range1.centerFreq = (range1.lowFreq + range1.highFreq) * 0.5f;
    }
}

//==============================================================================
// Analysis Methods

LinearTrackerEngine::ConflictAnalysis LinearTrackerEngine::analyzePattern(int patternIndex) const
{
    ConflictAnalysis analysis;
    
    if (patternIndex < 0 || patternIndex >= MAX_PATTERNS)
        return analysis;
    
    const auto& pattern = patterns[patternIndex];
    
    // Check for frequency conflicts
    for (int i = 0; i < MAX_TRACKS - 1; ++i)
    {
        for (int j = i + 1; j < MAX_TRACKS; ++j)
        {
            if (detectFrequencyMasking(i, j))
            {
                analysis.hasFrequencyMasking = true;
                analysis.conflictingTracks.push_back({i, j});
            }
        }
    }
    
    // Calculate pattern complexity
    int totalNotes = 0;
    for (int track = 0; track < MAX_TRACKS; ++track)
    {
        for (int row = 0; row < pattern.length; ++row)
        {
            if (!pattern.cells[track][row].isEmpty())
            {
                totalNotes++;
            }
        }
    }
    
    analysis.overallComplexity = static_cast<float>(totalNotes) / (MAX_TRACKS * pattern.length);
    
    return analysis;
}

void LinearTrackerEngine::optimizeForLinearDrumming(int patternIndex)
{
    if (patternIndex < 0 || patternIndex >= MAX_PATTERNS) return;
    
    // Resolve frequency conflicts
    resolveFrequencyConflicts();
    
    // Additional optimizations could include:
    // - Redistributing notes to avoid timing conflicts
    // - Adjusting velocities based on frequency masking
    // - Suggesting alternative arrangements
}