#pragma once
#include <JuceHeader.h>
#include <memory>
#include <atomic>
#include <vector>

/**
 * Sample Masking Engine - Revolutionary Paint-over-Sample System for Beatmakers
 * 
 * ARTEFACT's vintage-styled sample masher with spectral masking and tempo sync.
 * Perfect for beatmakers working with one-shots, hi-hats, textures, and loops.
 * 
 * Core Innovation:
 * - Paint strokes modulate sample playback in real-time
 * - Automatic tempo detection and host BPM synchronization
 * - Spectral masking with vintage hardware aesthetic
 * - Each stroke creates a "mask" that affects the underlying sample
 * - Perfect for creating evolving, organic drum patterns
 * - Supports polyrhythmic variations through multiple paint layers
 * 
 * New Beatmaker Features:
 * - Vintage SP-1200/MPC aesthetic with modern DSP
 * - Host tempo sync with automatic time-stretching
 * - Quantized paint strokes (1/4, 1/8, 1/16, 1/32 notes)
 * - Spectral frequency masking for surgical sample editing
 * - Secret sauce processing for that "magic" sound
 */
class SampleMaskingEngine
{
public:
    SampleMaskingEngine();
    ~SampleMaskingEngine();
    
    //==============================================================================
    // Audio Processing
    
    void prepareToPlay(double sampleRate, int samplesPerBlock, int numChannels);
    void processBlock(juce::AudioBuffer<float>& buffer);
    void releaseResources();
    
    // Host tempo sync (NEW for beatmakers!)
    void setHostTempo(double bpm);
    void setHostTimeSignature(int numerator, int denominator);
    void setHostPosition(double ppqPosition, bool isPlaying);
    double getHostTempo() const { return hostTempo.load(); }
    
    //==============================================================================
    // Sample Loading & Management
    
    struct LoadResult
    {
        bool success = false;
        juce::String errorMessage;
        juce::String fileName;
        double lengthSeconds = 0.0;
        int sampleRate = 0;
        int channels = 0;
    };
    
    LoadResult loadSample(const juce::File& sampleFile);
    void loadSample(const juce::AudioBuffer<float>& sampleBuffer, double sourceSampleRate);
    void clearSample();
    
    bool hasSample() const { return sampleBuffer != nullptr; }
    juce::String getCurrentSampleName() const { return currentSampleName; }
    double getSampleLengthSeconds() const;
    
    // Tempo detection and sync (NEW!)
    struct TempoInfo
    {
        double detectedBPM = 120.0;
        double confidence = 0.0f;        // 0.0-1.0
        bool isTempoStable = false;
        double beatPositions[4] = {0.0}; // First 4 beat positions
        int timeSignature = 4;           // Beats per bar
    };
    
    TempoInfo detectSampleTempo();
    void setSampleTempo(double bpm);
    void enableTempoSync(bool enabled) { tempoSyncEnabled.store(enabled); }
    bool isTempoSyncEnabled() const { return tempoSyncEnabled.load(); }
    
    // Time-stretching for tempo sync
    enum class StretchMode
    {
        PreservePitch,    // Classic time-stretching (default)
        Granular,         // Granular synthesis stretching
        SpectralPreserve, // Formant-preserving spectral
        Vintage          // Lo-fi stretching with character
    };
    
    void setTimeStretchMode(StretchMode mode) { stretchMode.store(mode); }
    void setTimeStretchQuality(float quality) { stretchQuality.store(juce::jlimit(0.0f, 1.0f, quality)); }
    
    // Sample playback control
    void startPlayback();
    void stopPlayback();
    void pausePlayback();
    void setLooping(bool shouldLoop) { isLooping.store(shouldLoop); }
    void setPlaybackSpeed(float speed) { playbackSpeed.store(juce::jlimit(0.1f, 4.0f, speed)); }
    void setPlaybackPosition(float normalizedPosition); // 0.0-1.0
    
    //==============================================================================
    // Paint Masking System - The Revolutionary Part!
    
    enum class MaskingMode
    {
        Volume,          // Paint controls volume/amplitude
        Filter,          // Paint controls filter cutoff
        Pitch,           // Paint controls pitch shift
        Granular,        // Paint controls granular position/size
        Reverse,         // Paint triggers reverse playback
        Chop,            // Paint creates rhythmic chopping
        Stutter,         // Paint creates stutter effects
        Ring,            // Paint controls ring modulation
        Distortion,      // Paint controls distortion amount
        Delay            // Paint controls delay feedback/time
    };
    
    struct PaintMask
    {
        juce::uint32 maskId;
        MaskingMode mode = MaskingMode::Volume;
        juce::Path paintPath;
        juce::Colour maskColor = juce::Colours::white;
        float intensity = 1.0f;      // Overall mask strength
        float fadeInTime = 0.01f;    // Seconds to fade in effect
        float fadeOutTime = 0.1f;    // Seconds to fade out effect
        bool isActive = true;
        juce::uint32 creationTime;
        
        // Mode-specific parameters using variant instead of union
        struct VolumeParams { float minLevel = 0.0f; float maxLevel = 1.0f; };
        struct FilterParams { float minCutoff = 100.0f; float maxCutoff = 8000.0f; float resonance = 0.0f; };
        struct PitchParams { float minSemitones = -12.0f; float maxSemitones = 12.0f; };
        struct GranularParams { float grainSize = 0.1f; float overlap = 0.5f; };
        struct ChopParams { float chopRate = 16.0f; float chopDepth = 1.0f; };
        struct StutterParams { float stutterRate = 8.0f; float stutterLength = 0.125f; };
        struct RingParams { float frequency = 40.0f; float depth = 0.5f; };
        struct DistortionParams { float drive = 1.0f; float mix = 0.5f; };
        struct DelayParams { float delayTime = 0.25f; float feedback = 0.3f; float mix = 0.3f; };
        
        // Simple parameter storage - just the most common values
        float param1 = 0.0f;
        float param2 = 1.0f; 
        float param3 = 0.0f;
        
        PaintMask() : creationTime(juce::Time::getMillisecondCounter()) 
        {
            // Default parameters for volume mode
        }
    };
    
    // Paint mask management
    juce::uint32 createPaintMask(MaskingMode mode, juce::Colour color = juce::Colours::white);
    void addPointToMask(juce::uint32 maskId, float x, float y, float pressure);
    void finalizeMask(juce::uint32 maskId);
    void removeMask(juce::uint32 maskId);
    void clearAllMasks();
    
    // Mask configuration
    void setMaskMode(juce::uint32 maskId, MaskingMode mode);
    void setMaskIntensity(juce::uint32 maskId, float intensity);
    void setMaskParameters(juce::uint32 maskId, float param1, float param2, float param3 = 0.0f);
    
    // Active masks info
    int getNumActiveMasks() const { return static_cast<int>(activeMasks.size()); }
    std::vector<juce::uint32> getActiveMaskIds() const;
    
    //==============================================================================
    // Real-Time Paint Interface
    
    // Direct paint control (for real-time performance)
    void beginPaintStroke(float x, float y, MaskingMode mode = MaskingMode::Volume);
    void updatePaintStroke(float x, float y, float pressure = 1.0f);
    void endPaintStroke();
    
    // Canvas coordinate system
    void setCanvasSize(float width, float height);
    void setTimeRange(float startSeconds, float endSeconds);
    
    // Coordinate conversion
    float canvasXToSampleTime(float x) const;
    float sampleTimeToCanvasX(float timeSeconds) const;
    
    // SAFETY: Preparation guard
    bool prepared() const { return isPrepared.load(std::memory_order_acquire); }
    
    //==============================================================================
    // Advanced Features
    
    // Polyrhythmic layers - multiple simultaneous masking patterns
    void createLayer(const juce::String& layerName);
    void setActiveLayer(const juce::String& layerName);
    void removeLayer(const juce::String& layerName);
    juce::StringArray getLayerNames() const;
    
    // Quantization for rhythmic precision (ENHANCED for beatmakers!)
    enum class QuantizeGrid
    {
        Off = 0,
        Whole = 1,     // 1/1 - Whole note
        Half = 2,      // 1/2 - Half note  
        Quarter = 4,   // 1/4 - Quarter note (most common)
        Eighth = 8,    // 1/8 - Eighth note
        Sixteenth = 16,// 1/16 - Sixteenth note
        ThirtySecond = 32, // 1/32 - Thirty-second note
        QuarterTriplet = 6,   // 1/4 triplet (3 notes per beat)
        EighthTriplet = 12,   // 1/8 triplet (6 notes per beat)
        DottedQuarter = 100,  // Dotted quarter (handled specially)
        DottedEighth = 101    // Dotted eighth (handled specially)
    };
    
    void setQuantization(QuantizeGrid grid);
    void setQuantizationStrength(float strength); // 0.0 = off, 1.0 = full snap
    void setSwingAmount(float swing); // 0.0 = straight, 1.0 = full swing
    
    // Performance features
    void recordMaskAutomation(bool shouldRecord) { isRecordingAutomation.store(shouldRecord); }
    void playbackAutomation(bool shouldPlayback) { isPlayingAutomation.store(shouldPlayback); }
    
private:
    //==============================================================================
    // Sample Storage & Playback
    
    std::unique_ptr<juce::AudioBuffer<float>> sampleBuffer;
    juce::String currentSampleName;
    double sourceSampleRate = 44100.0;
    double currentSampleRate = 44100.0;
    
    // Playback state
    std::atomic<double> playbackPosition{0.0};
    std::atomic<float> playbackSpeed{1.0f};
    std::atomic<bool> isPlaying{false};
    std::atomic<bool> isLooping{true};
    
    //==============================================================================
    // Masking System Implementation
    
    std::vector<PaintMask> activeMasks;
    juce::uint32 nextMaskId = 1;
    std::unique_ptr<PaintMask> currentPaintMask;

    // Lock-free snapshot of active masks for the audio thread
    // UI thread rebuilds a shared_ptr snapshot after any mutation under maskLock
    std::shared_ptr<const std::vector<PaintMask>> activeMasksSnapshot; // C++17 with free function atomics
    void rebuildActiveMasksSnapshotLocked();
    
    // Canvas coordinate system
    float canvasWidth = 1000.0f;
    float canvasHeight = 500.0f;
    float timeRangeStart = 0.0f;
    float timeRangeEnd = 4.0f; // 4 seconds default
    
    //==============================================================================
    // Real-Time Processing Effects
    
    // Filter for filter-mode masks
    struct MaskFilter
    {
        float cutoff = 1000.0f;
        float resonance = 0.0f;
        float low = 0.0f, band = 0.0f, high = 0.0f;
        float f = 0.0f, fb = 0.0f;
        
        void setParams(float newCutoff, float newResonance, double sampleRate);
        float process(float input);
    };
    
    // Granular engine for granular-mode masks
    struct GranularProcessor
    {
        struct Grain
        {
            double position = 0.0;
            double size = 0.1;
            double playbackPos = 0.0;
            float envelope = 0.0f;
            bool isActive = false;
        };
        
        static constexpr int MAX_GRAINS = 64;
        std::array<Grain, MAX_GRAINS> grains;
        int nextGrainIndex = 0;
        
        void triggerGrain(double samplePosition, double grainSize, double overlap);
        float processGrains(const juce::AudioBuffer<float>& source, double currentPos);
    };
    
    // Delay line for delay-mode masks
    struct DelayLine
    {
        std::vector<float> buffer;
        int writePos = 0;
        int maxDelayInSamples = 0;
        
        void setMaxDelay(double maxDelaySeconds, double sampleRate);
        void write(float input);
        float read(float delayInSamples);
        float readInterpolated(float delayInSamples);
    };
    
    MaskFilter maskFilter;
    GranularProcessor granularProcessor;
    DelayLine delayLine;
    
    //==============================================================================
    // Mask Application Engine
    
    float calculateMaskInfluence(const PaintMask& mask, double currentTimeSeconds) const;
    float applyVolumeMask(const PaintMask& mask, float input, double timeSeconds);
    float applyFilterMask(const PaintMask& mask, float input, double timeSeconds);
    float applyPitchMask(const PaintMask& mask, float input, double timeSeconds);
    float applyGranularMask(const PaintMask& mask, float input, double timeSeconds);
    float applyChopMask(const PaintMask& mask, float input, double timeSeconds);
    float applyStutterMask(const PaintMask& mask, float input, double timeSeconds);
    float applyRingMask(const PaintMask& mask, float input, double timeSeconds);
    float applyDistortionMask(const PaintMask& mask, float input, double timeSeconds);
    float applyDelayMask(const PaintMask& mask, float input, double timeSeconds);
    
    //==============================================================================
    // Polyrhythmic Layer System
    
    struct MaskLayer
    {
        juce::String name;
        std::vector<PaintMask> masks;
        float volume = 1.0f;
        bool isMuted = false;
        bool isSoloed = false;
    };
    
    std::vector<MaskLayer> maskLayers;
    int activeMaskLayer = 0;
    
    //==============================================================================
    // Tempo Synchronization & Timing (NEW!)
    
    // Host tempo tracking
    std::atomic<double> hostTempo{120.0};
    std::atomic<double> hostPPQPosition{0.0};
    std::atomic<bool> hostIsPlaying{false};
    int hostTimeSignatureNumerator = 4;
    int hostTimeSignatureDenominator = 4;
    
    // Sample tempo detection
    TempoInfo currentTempoInfo;
    std::atomic<double> sampleTempo{120.0};
    std::atomic<bool> tempoSyncEnabled{false};
    
    // Time-stretching
    std::atomic<StretchMode> stretchMode{StretchMode::PreservePitch};
    std::atomic<float> stretchQuality{0.8f};
    double timeStretchRatio = 1.0; // hostTempo / sampleTempo
    
    // Spectral processing for tempo detection and masking
    class SpectralAnalyzer
    {
    public:
        void analyzeBuffer(const juce::AudioBuffer<float>& buffer, double sampleRate);
        TempoInfo getTempoInfo() const { return tempoInfo; }
        std::vector<float> getSpectralMagnitudes() const { return magnitudes; }
        
    private:
        static constexpr int FFT_ORDER = 11; // 2048 samples
        static constexpr int FFT_SIZE = 1 << FFT_ORDER;
        
        std::unique_ptr<juce::dsp::FFT> fft;
        std::vector<float> fftData;
        std::vector<float> window;
        std::vector<float> magnitudes;
        TempoInfo tempoInfo;
        
        void detectTempo(const std::vector<float>& spectrum, double sampleRate);
        void detectBeats(const juce::AudioBuffer<float>& buffer, double sampleRate);
    };
    
    std::unique_ptr<SpectralAnalyzer> spectralAnalyzer;
    
    // Quantization & Timing
    std::atomic<QuantizeGrid> quantizeGrid{QuantizeGrid::Off};
    std::atomic<float> quantizationStrength{0.0f};
    std::atomic<float> swingAmount{0.0f};
    
    float quantizeTime(float timeSeconds) const;
    double calculateQuantizedBeatTime(double timeSeconds) const;
    void updateTimeStretchRatio(); // Helper method to update stretch ratio
    
    //==============================================================================
    // Automation Recording/Playback
    
    struct AutomationPoint
    {
        double timeSeconds;
        juce::uint32 maskId;
        float x, y, pressure;
    };
    
    std::vector<AutomationPoint> recordedAutomation;
    std::atomic<bool> isRecordingAutomation{false};
    std::atomic<bool> isPlayingAutomation{false};
    
    //==============================================================================
    // Performance & Threading
    
    juce::CriticalSection maskLock;
    juce::AudioFormatManager formatManager;
    
    // Performance monitoring
    std::atomic<float> cpuUsage{0.0f};
    juce::Time lastProcessTime;
    
    // SAFETY: Track initialization state
    std::atomic<bool> isPrepared{false};
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SampleMaskingEngine)
};
