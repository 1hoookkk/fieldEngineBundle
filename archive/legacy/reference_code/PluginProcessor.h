#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_core/juce_core.h>
#include "../preset_loader_json.hpp"
#include "../Source/Core/DSPEngine.h"
#include "../Source/MusicFX/Chordifier.h"
#include "../Source/MusicFX/EnvFollower.h"
#include "../Source/preset/A2KRuntime.h"

// Forward declarations from DSPEngine for shape loading
using PolePair = fe::PolePair;
constexpr int ZPLANE_N_SECTIONS = fe::ZPLANE_N_SECTIONS;

//==============================================================================
class FieldEngineAudioProcessor  : public juce::AudioProcessor,
                                     private juce::AudioProcessorValueTreeState::Listener
{
public:
    //==============================================================================
    FieldEngineAudioProcessor();
    ~FieldEngineAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Parameter tree for DAW automation
    juce::AudioProcessorValueTreeState apvts;

    // EMU bank management (will be used by editor)
    fe::DSPEngine dspEngine;
    fe::Chordifier chordifier; // Musical harmonicizer
    std::atomic<bool> panicMute{false}; // Safety: mute on invalid samples

    bool isEngineInitialized() const { return engineInitialized.load(); }

    // WAV recording control (called from UI)
    void startRecording();
    void stopRecording();
    bool getIsRecording() const { return isRecording.load(); }
    
    // MIDI from UI
    void pushMidiNote(bool isNoteOn, int noteNumber, float velocity = 1.0f);
    
    // ---- Visual FIFO (RT-safe audio → GUI data pipe) ----
    // Made public for editor access
    static constexpr int kVizFftSize = 512;      // FFT size
    static constexpr int kVizBufSize = 8192;     // Ring buffer size
    juce::AbstractFifo vizFifo { kVizBufSize };
    juce::AudioBuffer<float> vizBuffer { 1, kVizBufSize };
    
    // Real-time meters
    std::atomic<float> vizRmsIn  { 0.0f };
    std::atomic<float> vizRmsOut { 0.0f };
    std::atomic<float> vizPeakIn { 0.0f };
    std::atomic<float> vizPeakOut{ 0.0f };

private:
    //==============================================================================
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void updateHostTempo();

    // AudioProcessorValueTreeState::Listener
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    // Engine state
    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;
    double lastBpm = 120.0;
    std::atomic<bool> engineInitialized{false};

    // Fail-safe dry buffer for passthrough on bad coefficients/silence
    juce::AudioBuffer<float> tempDry;

    // Parallel routing buffers for Filter/Chordifier blend
    juce::AudioBuffer<float> tempFilter, tempChord;
    static constexpr int kModChunk = 64; // Sub-block control rate

    // Thread-safe Z-plane shape loading system
    std::array<PolePair, ZPLANE_N_SECTIONS> audioShapeA_banks[2];
    std::array<PolePair, ZPLANE_N_SECTIONS> audioShapeB_banks[2];
    std::atomic<int> audioShapeBankIndex{0}; // 0 or 1
    int audioLocalBankIndex = 0; // audio thread local copy

    // Shape loading helpers
    void loadShapePreset(const juce::String& presetId);
    void loadDefaultShapes();
    void forceParameterSync();
    float theta48_to_thetaFs(float theta48, float fs) const;
    std::array<PolePair, ZPLANE_N_SECTIONS> parseShapeFromJson(const juce::var& shapeVar, float fs) const;

    // ---- Built-in morph LFO (kept tiny & RT-safe) ----
    struct SimpleLFO {
        void prepare (double fs) { sampleRate = fs; }
        void setRateHz (float r) { rateHz = juce::jlimit (0.001f, 20.0f, r); incr = rateHz / (float) sampleRate; }
        void setPhaseOffset (float degrees) { phaseOffset = juce::jlimit (0.0f, 360.0f, degrees) / 360.0f; }
        inline float tick() {
            // single-sample band-limited-ish sine (cheap)
            float p = phase + phaseOffset;
            if (p >= 1.f) p -= 1.f;
            float v = std::sin (juce::MathConstants<float>::twoPi * p);
            phase += incr; if (phase >= 1.f) phase -= 1.f;
            return v;
        }
        double sampleRate = 48000.0;
        float  rateHz = 0.25f, incr = rateHz / 48000.0f;
        float  phase = 0.0f, phaseOffset = 0.0f;
    };

    SimpleLFO lfoL, lfoR;
    float     lfoDepth = 0.25f;      // 0..1 depth (applied to morph)
    float     baseMorph = 0.0f;      // host/GUI morph (center)
    bool      lfoEnabled = true;     // always on for "alive" feel

    // ---- Envelope → Morph modulation (program/sidechain-ready hook) ----
    struct EnvMod {
        fe::EnvFollower env;
        float depth = 0.35f;      // 0..1, scales morph offset
        float bias  = 0.15f;      // lifts low levels so quiet sources still move
        bool  invert = false;     // flip polarity if desired
        float last = 0.0f;        // cached last env (0..1)
        void prepare(double fs, float atkMs=5.f, float relMs=80.f) { env.reset(fs, atkMs, relMs); }
        inline float processSample(float x){
            float e = env.process(x);   // rectified + AR env
            last = juce::jlimit(0.f, 1.f, bias + (invert ? (1.f - e) : e));
            return last;
        }
    } envMod;

    // Chunk size for inexpensive control-rate modulation (defined above)
    
    // Test tone generator for audition feature
    struct TestTone {
        double sr = 48000;
        double ph = 0;
        float freq = 220.0f;
        
        void prepare(double fs) { 
            sr = fs; 
            ph = 0; 
        }
        
        void setFrequency(float f) {
            freq = f;
        }
        
        void render(float* L, float* R, int n, int chs, int type, float gainDb) {
            const float g = juce::Decibels::decibelsToGain(gainDb);
            if (type == 0) { // Sine
                const double inc = 2.0 * juce::MathConstants<double>::pi * freq / sr;
                for (int i = 0; i < n; ++i) { 
                    float s = (float)std::sin(ph); 
                    ph += inc; 
                    if (ph > 2 * juce::MathConstants<double>::pi) 
                        ph -= 2 * juce::MathConstants<double>::pi;
                    if (chs > 1) { 
                        L[i] += g * s; 
                        R[i] += g * s; 
                    } else { 
                        L[i] += g * s; 
                    } 
                }
            } else { // Noise
                juce::Random& rng = juce::Random::getSystemRandom();
                for (int i = 0; i < n; ++i) { 
                    float s = rng.nextFloat() * 2.0f - 1.0f;
                    if (chs > 1) { 
                        L[i] += g * s; 
                        R[i] += g * s; 
                    } else { 
                        L[i] += g * s; 
                    } 
                }
            }
        }
    } testTone;
    
    // WAV recording
    std::unique_ptr<juce::AudioFormatWriter> wavWriter;
    juce::File wavOutputFile;
    std::atomic<bool> isRecording{false};
    juce::AudioBuffer<float> recordBuffer;
    
    // MIDI from UI
    struct MidiNoteEvent {
        bool isNoteOn;
        int noteNumber;
        float velocity;
    };
    juce::AbstractFifo midiEventFifo{32};
    std::array<MidiNoteEvent, 32> midiEventBuffer;

    // ---- Audity 2000 Integration ----
    a2k::BankData audityBank;
    int currentPresetA = 0;
    int currentPresetB = 1;
    int sampleCounter = 0;
    static constexpr int controlRateSamples = 64; // Control rate processing

    void loadAudityBank(); // Load authentic EMU bank data

    // Helper to push samples to viz buffer (called from processBlock)
    void pushVizSamples(const float* L, const float* R, int numSamples);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FieldEngineAudioProcessor)
};