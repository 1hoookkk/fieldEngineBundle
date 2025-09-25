#pragma once
#include <JuceHeader.h>
#include <memory>
#include <atomic>
#include <array>
#include <vector>

/**
 * Linear Tracker Engine - Revolutionary New Sequencing Concept
 * 
 * Combines classic tracker-style linear sequencing with modern paint interface
 * and linear drumming principles. Each vertical "track" represents a frequency
 * range or drum type, ensuring no frequency masking or interference.
 * 
 * Innovation:
 * - Paint interface replaces hex entry from classic trackers
 * - Linear arrangement prevents mud and frequency conflicts  
 * - Real-time polyrhythmic pattern creation
 * - Visual representation of complex rhythms
 * - Automatic frequency separation and smart voice allocation
 */
class LinearTrackerEngine
{
public:
    LinearTrackerEngine();
    ~LinearTrackerEngine();
    
    //==============================================================================
    // Audio Processing
    
    void prepareToPlay(double sampleRate, int samplesPerBlock, int numChannels);
    void processBlock(juce::AudioBuffer<float>& buffer);
    void releaseResources();
    
    //==============================================================================
    // Tracker Pattern System
    
    static constexpr int MAX_TRACKS = 16;        // Like classic trackers
    static constexpr int MAX_PATTERN_LENGTH = 64; // Rows per pattern
    static constexpr int MAX_PATTERNS = 256;     // Total patterns
    
    struct TrackerCell
    {
        int note = -1;           // MIDI note (-1 = empty)
        int instrument = -1;     // Instrument/sample ID (-1 = empty)
        int volume = 64;         // Volume (0-64, tracker style)
        int effect = 0;          // Effect command
        int effectParam = 0;     // Effect parameter
        
        // Paint-specific data
        float paintPressure = 1.0f;  // From paint stroke
        juce::Colour paintColor = juce::Colours::white;
        float paintVelocity = 0.0f;  // From stroke speed
        
        bool isEmpty() const { return note == -1 && instrument == -1; }
        void clear() { *this = TrackerCell{}; }
    };
    
    struct TrackerPattern
    {
        TrackerCell cells[MAX_TRACKS][MAX_PATTERN_LENGTH];
        juce::String name = "Pattern";
        int length = 64;         // Actual pattern length
        float tempo = 120.0f;    // BPM for this pattern
        int rowsPerBeat = 4;     // Subdivision
        
        void clear();
        void resizePattern(int newLength);
    };
    
    //==============================================================================
    // Linear Drumming Frequency Assignment
    
    enum class DrumType
    {
        Kick = 0,        // 20-80 Hz
        Snare,           // 150-250 Hz + harmonics
        ClosedHat,       // 8-15 kHz
        OpenHat,         // 6-12 kHz  
        Crash,           // 3-8 kHz spread
        Ride,            // 4-10 kHz
        Tom1,            // 80-120 Hz
        Tom2,            // 60-100 Hz
        Tom3,            // 40-80 Hz
        Clap,            // 1-3 kHz
        Rim,             // 2-5 kHz
        Shaker,          // 10-16 kHz
        Percussion1,     // Custom range
        Percussion2,     // Custom range
        FX1,             // Effects/sweeps
        FX2              // Effects/sweeps
    };
    
    struct FrequencyRange
    {
        float lowFreq;
        float highFreq;
        float centerFreq;
        DrumType drumType;
        juce::Colour trackColor;
        juce::String trackName;
        
        bool doesOverlap(const FrequencyRange& other) const;
    };
    
    // Get/set frequency assignments for tracks
    void setTrackFrequencyRange(int trackIndex, DrumType drumType);
    void setCustomFrequencyRange(int trackIndex, float lowHz, float highHz);
    FrequencyRange getTrackFrequencyRange(int trackIndex) const;
    
    // Automatic linear arrangement (prevents frequency conflicts)
    void autoArrangeFrequencies();
    bool checkForFrequencyConflicts() const;
    void resolveFrequencyConflicts();
    
    //==============================================================================
    // Paint-to-Tracker Interface - The Revolutionary Part!
    
    struct PaintStroke
    {
        int trackIndex;
        int startRow;
        int endRow;
        std::vector<juce::Point<float>> points;
        float pressure;
        juce::Colour color;
        DrumType drumType;
        juce::uint32 timestamp;
    };
    
    // Paint interface for pattern creation
    void beginPaintStroke(float x, float y, float pressure, juce::Colour color);
    void updatePaintStroke(float x, float y, float pressure);
    void endPaintStroke();
    
    // Canvas coordinate conversion
    void setCanvasSize(float width, float height);
    int canvasXToRow(float x) const;
    int canvasYToTrack(float y) const;
    float rowToCanvasX(int row) const;
    float trackToCanvasY(int track) const;
    
    // Paint stroke interpretation
    void convertPaintToPattern(const PaintStroke& stroke);
    void generateNotesFromStroke(const PaintStroke& stroke, TrackerPattern& pattern);
    
    //==============================================================================
    // Pattern Management
    
    void setCurrentPattern(int patternIndex);
    int getCurrentPatternIndex() const { return currentPatternIndex.load(); }
    TrackerPattern& getCurrentPattern() { return patterns[currentPatternIndex.load()]; }
    const TrackerPattern& getCurrentPattern() const { return patterns[currentPatternIndex.load()]; }
    
    void copyPattern(int sourceIndex, int destIndex);
    void clearPattern(int patternIndex);
    void clearTrack(int patternIndex, int trackIndex);
    
    // Pattern sequencing
    void setPatternSequence(const std::vector<int>& sequence);
    void playPatternSequence(bool shouldPlay) { isSequencePlaying.store(shouldPlay); }
    
    //==============================================================================
    // Playback Control
    
    void startPlayback();
    void stopPlayback();
    void pausePlayback();
    void setPlaybackPosition(int row);
    void setTempo(float bpm);
    void setSwing(float swingAmount); // 0.0-1.0
    
    bool isPlaying() const { return isPlaybackActive.load(); }
    int getCurrentRow() const { return currentRow.load(); }
    float getTempo() const { return currentTempo.load(); }
    
    //==============================================================================
    // Instrument/Sample Assignment
    
    struct TrackerInstrument
    {
        juce::String name;
        std::unique_ptr<juce::AudioBuffer<float>> sampleBuffer;
        double sourceSampleRate = 44100.0;
        
        // Tracker-style parameters
        int volume = 64;         // 0-64
        int fineTune = 0;        // -128 to +127
        int relativeNote = 0;    // Semitone offset
        
        // Linear drumming optimization
        FrequencyRange frequencyRange;
        bool useFrequencyIsolation = true;
        
        // ADSR envelope
        float attack = 0.01f;
        float decay = 0.1f;
        float sustain = 0.8f;
        float release = 0.2f;
    };
    
    void loadInstrument(int instrumentIndex, const juce::File& sampleFile);
    void setInstrumentParameters(int instrumentIndex, const TrackerInstrument& params);
    TrackerInstrument& getInstrument(int instrumentIndex);
    
    //==============================================================================
    // Tracker Effects (Classic + Modern)
    
    enum class EffectCommand
    {
        None = 0x00,
        // Classic tracker effects
        Arpeggio = 0x00,
        SlideUp = 0x01,
        SlideDown = 0x02,
        SlideToNote = 0x03,
        Vibrato = 0x04,
        VolumeSlide = 0x0A,
        JumpToPattern = 0x0B,
        SetVolume = 0x0C,
        PatternBreak = 0x0D,
        SetSpeed = 0x0F,
        
        // Modern paint-derived effects
        PaintFilter = 0x10,      // Filter controlled by paint pressure
        PaintPitch = 0x11,       // Pitch bend from paint velocity
        PaintGrain = 0x12,       // Granular from paint texture
        PaintReverb = 0x13,      // Reverb from paint color
        PaintDistort = 0x14,     // Distortion from paint intensity
        PaintDelay = 0x15        // Delay from paint trail length
    };
    
    void setEffect(int patternIndex, int trackIndex, int row, EffectCommand effect, int param);
    void processEffect(const TrackerCell& cell, int trackIndex);
    
    //==============================================================================
    // Visual Feedback System
    
    struct VisualState
    {
        float currentRowHighlight = 0.0f;  // For scrolling highlight
        std::array<float, MAX_TRACKS> trackActivity; // VU meters per track
        std::vector<PaintStroke> recentStrokes; // For trail visualization
        juce::Rectangle<float> canvasBounds;
        
        // Real-time waveform display per track
        std::array<std::vector<float>, MAX_TRACKS> trackWaveforms;
    };
    
    VisualState getVisualState() const;
    
    //==============================================================================
    // Advanced Features
    
    // Polyrhythmic capabilities
    void setTrackLength(int trackIndex, int length); // Different track lengths
    void setTrackSpeed(int trackIndex, float speedMultiplier); // Different track speeds
    
    // Linear drumming analysis
    struct ConflictAnalysis
    {
        bool hasFrequencyMasking = false;
        bool hasTimingConflicts = false;
        std::vector<std::pair<int, int>> conflictingTracks;
        float overallComplexity = 0.0f; // 0.0-1.0
    };
    
    ConflictAnalysis analyzePattern(int patternIndex) const;
    void optimizeForLinearDrumming(int patternIndex);
    
    // Smart quantization with musical intelligence
    void setQuantizeStrength(float strength); // 0.0-1.0
    void setQuantizeSubdivision(int subdivision); // 1, 2, 4, 8, 16, 32
    void enableSwingQuantization(bool enable, float swingAmount = 0.66f);
    
private:
    //==============================================================================
    // Pattern Storage
    
    std::array<TrackerPattern, MAX_PATTERNS> patterns;
    std::atomic<int> currentPatternIndex{0};
    
    // Pattern sequencing
    std::vector<int> patternSequence;
    std::atomic<int> sequencePosition{0};
    std::atomic<bool> isSequencePlaying{false};
    
    //==============================================================================
    // Playback Engine
    
    std::atomic<bool> isPlaybackActive{false};
    std::atomic<int> currentRow{0};
    std::atomic<float> currentTempo{120.0f};
    std::atomic<float> swingAmount{0.0f};
    
    double sampleRate = 44100.0;
    int samplesPerBlock = 512;
    
    // Timing calculation
    double samplesPerRow = 0.0;
    double samplePosition = 0.0;
    double nextRowPosition = 0.0;
    
    void calculateTiming();
    void advancePlayback(int numSamples);
    void processRowTriggers();
    
    //==============================================================================
    // Voice Management (for polyphonic instruments)
    
    struct TrackerVoice
    {
        bool isActive = false;
        int trackIndex = -1;
        int instrumentIndex = -1;
        int midiNote = -1;
        float volume = 1.0f;
        
        // Sample playback
        double samplePosition = 0.0;
        double pitchRatio = 1.0;
        
        // Envelope
        float envelopeLevel = 0.0f;
        enum class EnvStage { Attack, Decay, Sustain, Release, Idle } envStage = EnvStage::Idle;
        
        // Effects state
        float vibratoPhase = 0.0f;
        float slideTarget = 0.0f;
        float currentPitch = 0.0f;
        
        void startNote(int track, int instrument, int note, float vel);
        void stopNote();
        float processEnvelope(float attack, float decay, float sustain, float release);
        float renderNextSample(const TrackerInstrument& instrument);
    };
    
    static constexpr int MAX_VOICES = 32;
    std::array<TrackerVoice, MAX_VOICES> voices;
    
    TrackerVoice* findFreeVoice();
    void triggerNote(int trackIndex, const TrackerCell& cell);
    
    //==============================================================================
    // Instrument Storage
    
    static constexpr int MAX_INSTRUMENTS = 64;
    std::array<TrackerInstrument, MAX_INSTRUMENTS> instruments;
    
    //==============================================================================
    // Canvas & Paint State
    
    float canvasWidth = 1000.0f;
    float canvasHeight = 600.0f;
    std::unique_ptr<PaintStroke> currentPaintStroke;
    std::vector<PaintStroke> recentPaintStrokes;
    
    // Frequency range assignments
    std::array<FrequencyRange, MAX_TRACKS> trackFrequencyRanges;
    
    //==============================================================================
    // Effect Processing
    
    void processTrackEffects(int trackIndex, juce::AudioBuffer<float>& buffer);
    void updateEffectParameters(TrackerVoice& voice, const TrackerCell& cell);
    
    //==============================================================================
    // Linear Drumming Intelligence
    
    bool detectFrequencyMasking(int track1, int track2) const;
    float calculateFrequencyOverlap(const FrequencyRange& range1, const FrequencyRange& range2) const;
    void separateConflictingTracks(int track1, int track2);
    
    //==============================================================================
    // Quantization Engine
    
    std::atomic<float> quantizeStrength{0.8f};
    std::atomic<int> quantizeSubdivision{4}; // 16th notes
    std::atomic<bool> swingQuantizeEnabled{false};
    
    int quantizeRowPosition(float paintX) const;
    float applySwing(float position) const;
    
    //==============================================================================
    // Performance & Threading
    
    juce::CriticalSection patternLock;
    juce::CriticalSection voiceLock;
    juce::AudioFormatManager formatManager;
    
    // Performance monitoring
    std::atomic<float> cpuUsage{0.0f};
    juce::Time lastProcessTime;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LinearTrackerEngine)
};