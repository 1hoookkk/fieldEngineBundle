/**
 * EMU Modulation System
 * Classic ADSR envelopes and LFOs with EMU Rompler characteristics
 * Paint canvas integration for expressive real-time control
 */

#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>
#include <cmath>

/**
 * EMU ADSR Envelope Generator
 * Models classic analog ADSR with EMU-style curves and behavior
 */
class EMUEnvelope
{
public:
    EMUEnvelope();
    ~EMUEnvelope() = default;
    
    // Envelope states
    enum EnvelopeState
    {
        Idle = 0,
        Attack,
        Decay, 
        Sustain,
        Release
    };
    
    // Audio processing
    void prepareToPlay(double sampleRate);
    float getNextSample();
    void processBlock(float* buffer, int numSamples);
    void reset();
    
    // Envelope control
    void noteOn();                               // Trigger envelope
    void noteOff();                              // Release envelope
    void kill();                                 // Force to idle immediately
    
    // ADSR parameters
    void setAttack(float timeInSeconds);         // 0.001 - 10.0 seconds
    void setDecay(float timeInSeconds);          // 0.001 - 10.0 seconds 
    void setSustain(float level);                // 0.0 - 1.0
    void setRelease(float timeInSeconds);        // 0.001 - 10.0 seconds
    
    // EMU character controls
    void setAttackCurve(float curve);            // -1.0 to +1.0 (exponential curve)
    void setDecayReleaseCurve(float curve);      // -1.0 to +1.0
    void setVelocityAmount(float amount);        // 0.0 - 1.0 velocity sensitivity
    void setKeyTrackAmount(float amount);        // 0.0 - 1.0 key tracking
    
    // Paint canvas integration
    void setPaintModulation(float pressure, float y);  // Real-time modulation from paint
    void setPaintMapping(int attackMap, int releaseMap); // What paint controls
    
    // State inquiry
    bool isActive() const;
    float getCurrentLevel() const { return currentLevel; }
    EnvelopeState getState() const { return currentState; }
    
    // Visual feedback (for UI displays)
    struct EnvelopeData
    {
        float attackTime, decayTime, sustainLevel, releaseTime;
        float currentLevel;
        EnvelopeState state;
        float timeInState;
    };
    EnvelopeData getEnvelopeData() const;
    
private:
    // Envelope parameters
    float attackTime = 0.1f;
    float decayTime = 0.3f;
    float sustainLevel = 0.7f;
    float releaseTime = 0.5f;
    
    // Curve shaping
    float attackCurve = 0.0f;        // Linear by default
    float decayReleaseCurve = 0.0f;  // Linear by default
    
    // Current state
    EnvelopeState currentState = Idle;
    float currentLevel = 0.0f;
    float targetLevel = 0.0f;
    float stateTime = 0.0f;
    float sampleRate = 44100.0;
    
    // Modulation
    float velocityAmount = 1.0f;
    float keyTrackAmount = 0.0f;
    float paintPressureMod = 0.0f;
    float paintYMod = 0.0f;
    int attackPaintMapping = 0;      // 0=None, 1=Pressure, 2=Y-axis
    int releasePaintMapping = 0;
    
    // Current MIDI info
    float currentVelocity = 1.0f;
    int currentNote = 60;
    
    // Internal methods
    void updateState();
    float calculateRate(float timeInSeconds, float curve = 0.0f) const;
    float applyCurve(float linear, float curve) const;
    void applyPaintModulation();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EMUEnvelope)
};

/**
 * EMU LFO (Low Frequency Oscillator)  
 * Classic analog-style LFO with EMU waveforms and sync options
 */
class EMULFO
{
public:
    EMULFO();
    ~EMULFO() = default;
    
    // LFO waveforms (classic EMU selection)
    enum Waveform
    {
        Sine = 0,
        Triangle = 1,
        Square = 2,
        Sawtooth = 3,
        ReverseSaw = 4,
        SampleAndHold = 5,
        Noise = 6
    };
    
    // Audio processing
    void prepareToPlay(double sampleRate);
    float getNextSample();
    void processBlock(float* buffer, int numSamples);
    void reset();
    void sync();                                 // Reset phase to 0
    
    // LFO parameters
    void setRate(float hz);                      // 0.01 - 100 Hz
    void setDepth(float depth);                  // 0.0 - 1.0
    void setWaveform(Waveform wave);
    void setPhaseOffset(float phase);            // 0.0 - 1.0 (0-360 degrees)
    void setSymmetry(float symmetry);            // 0.0 - 1.0 (pulse width for square)
    
    // Sync and timing
    void setBPMSync(bool enabled);
    void setBPMRate(float bpm, int division);    // Division: 1=whole, 2=half, 4=quarter, etc.
    void setFadeIn(float fadeTime);              // Fade in time after note-on
    
    // Paint canvas integration
    void setPaintModulation(float x, float y, float pressure);
    void setPaintMapping(int rateMap, int depthMap, int waveMap);
    
    // EMU character
    void setVintageMode(bool enabled);           // Adds analog drift and imperfections
    void setTempoSync(bool enabled);             // Sync to host tempo
    
    // State inquiry
    float getCurrentValue() const { return currentValue; }
    float getPhase() const { return phase; }
    bool isActive() const { return depth > 0.001f; }
    
private:
    // LFO parameters
    float rate = 1.0f;              // Hz
    float depth = 0.5f;             // 0.0-1.0
    Waveform waveform = Sine;
    float phaseOffset = 0.0f;
    float symmetry = 0.5f;
    
    // Timing and sync
    bool bpmSyncEnabled = false;
    float currentBPM = 120.0f;
    int syncDivision = 4;           // Quarter note
    bool tempoSyncEnabled = false;
    
    // Current state
    float phase = 0.0f;
    float currentValue = 0.0f;
    float phaseIncrement = 0.0f;
    double sampleRate = 44100.0;
    
    // Fade in
    float fadeInTime = 0.0f;
    float fadeInCounter = 0.0f;
    float fadeInGain = 1.0f;
    
    // Paint modulation
    float paintRateMod = 0.0f;
    float paintDepthMod = 0.0f;
    float paintWaveMod = 0.0f;
    int ratePaintMapping = 0;
    int depthPaintMapping = 0;
    int wavePaintMapping = 0;
    
    // Vintage character
    bool vintageMode = true;
    juce::Random vintageRandom;
    float analogDrift = 0.0f;
    
    // Internal methods
    void updatePhaseIncrement();
    float generateWaveform(float phase) const;
    float applySampleAndHold();
    void applyPaintModulation();
    void updateVintageCharacter();
    
    // Static wavetables for efficiency
    static constexpr int WAVETABLE_SIZE = 1024;
    static std::array<float, WAVETABLE_SIZE> sineTable;
    static std::array<float, WAVETABLE_SIZE> triangleTable;
    static std::array<float, WAVETABLE_SIZE> sawTable;
    static bool tablesInitialized;
    static void initializeWavetables();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EMULFO)
};

/**
 * EMU Modulation Matrix
 * Routes modulation sources to destinations with EMU-style flexibility
 */
class EMUModMatrix
{
public:
    EMUModMatrix();
    ~EMUModMatrix() = default;
    
    // Modulation sources
    enum ModSource
    {
        None = 0,
        LFO1, LFO2,
        Envelope1, Envelope2, Envelope3,
        PaintX, PaintY, PaintPressure, PaintColor,
        Velocity, KeyTrack, PitchBend, ModWheel,
        Aftertouch, Random
    };
    
    // Modulation destinations  
    enum ModDestination
    {
        FilterCutoff = 0, FilterResonance, FilterType,
        SamplePitch, SampleVolume, SampleStart,
        LFO1Rate, LFO1Depth, LFO2Rate, LFO2Depth,
        EnvAttack, EnvDecay, EnvSustain, EnvRelease,
        ArpRate, ArpRange, ArpPattern
    };
    
    // Matrix configuration
    void setConnection(int slot, ModSource source, ModDestination dest, float amount);
    void clearConnection(int slot);
    void clearAllConnections();
    
    // Real-time processing
    void updateSources(const std::array<float, 16>& sourceValues);
    float getModulationFor(ModDestination destination) const;
    void reset();
    
    // Paint canvas as modulation source
    void updatePaintSources(float x, float y, float pressure, juce::Colour color);
    
    // Preset connections (classic EMU-style patches)
    void loadPresetMatrix(int presetNumber);
    void saveCurrentMatrix(int slotNumber);
    
private:
    // Matrix configuration
    struct Connection
    {
        ModSource source = None;
        ModDestination destination = FilterCutoff;
        float amount = 0.0f;
        bool active = false;
    };
    
    static constexpr int MAX_CONNECTIONS = 16;
    std::array<Connection, MAX_CONNECTIONS> connections;
    
    // Current modulation values
    std::array<float, 16> currentSources;
    std::array<float, 32> destinationValues;
    
    // Paint sources (cached)
    float paintX = 0.0f, paintY = 0.0f, paintPressure = 0.0f;
    float paintHue = 0.0f, paintSaturation = 0.0f, paintBrightness = 0.0f;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EMUModMatrix)
};