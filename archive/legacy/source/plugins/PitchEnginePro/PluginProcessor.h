#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <pitchengine_dsp/pitchengine_dsp.h>

class PitchEngineAudioProcessor : public juce::AudioProcessor
{
public:
    PitchEngineAudioProcessor();
    ~PitchEngineAudioProcessor() override = default;

    // Core
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    // Layout
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    // Editor
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;

    // Info
    const juce::String getName() const override { return "pitchEngine Pro"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // Programs (single program)
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    // State
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Params
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts { *this, nullptr, "params", createLayout() };

    // Host latency report (0 in Track mode)
    int getLatencySamples() const { return reportedLatencySamples; }

    // Race-free meter system using double-buffered frames
    struct MeterFrame {
        float rmsL = 0.0f, rmsR = 0.0f;
        bool clipL = false, clipR = false;
        uint32_t seq = 0;
    };

    std::array<MeterFrame, 2> meterBuffer{};
    std::atomic<uint32_t> meterWriteIndex{0};

    // Thread-safe meter publishing (audio thread)
    void pushMeters(float rmsL, float rmsR, bool clipL, bool clipR) noexcept {
        auto wi = meterWriteIndex.load(std::memory_order_relaxed);
        auto& frame = meterBuffer[wi ^ 1]; // Write to opposite slot
        frame.rmsL = rmsL;
        frame.rmsR = rmsR;
        frame.clipL = clipL;
        frame.clipR = clipR;
        frame.seq = frame.seq + 1; // Increment sequence last
        meterWriteIndex.store(wi ^ 1, std::memory_order_release);
    }

    // Thread-safe meter reading (UI thread)
    MeterFrame readMeters() const {
        auto ri = meterWriteIndex.load(std::memory_order_acquire);
        return meterBuffer[ri]; // Single coherent snapshot
    }

    // Access analyzer for UI visualization
    const Analyzer& getAnalyzer() const { return analyzer; }

    // A/B State Management
    void saveToA() { saveCurrentState(stateA); }
    void saveToB() { saveCurrentState(stateB); }
    void recallA() { recallState(stateA); }
    void recallB() { recallState(stateB); }
    void copyAToB() { stateB = stateA; }
    void copyBToA() { stateA = stateB; }
    bool hasStateA() const { return stateA.isValid(); }
    bool hasStateB() const { return stateB.isValid(); }

private:
    // Engines
    ZPlaneStyle  zplane;     // already used
    SimpleEMUZPlane emuZPlane;  // Simplified EMU with same character
    AuthenticEMUZPlane authenticEmu; // Real EMU hardware filter
    Shifter      shifter;
    Snapper      snapper;
    Analyzer     analyzer;
    PitchEngine  pitchEngine;
    FormantRescue formantRescue;

    // Advanced VoxZPlane pipeline
    vox::Brain   voxBrain;

    // Utils
    AutoGain       autoGain;
    SibilantGuard  sibGuard;

    // Runtime
    double fs = 48000.0;
    int    blockSize = 0;
    int    reportedLatencySamples = 0;
    juce::AudioBuffer<float> dry; // for bypass crossfade
    // Preallocated scratch to avoid per-block allocation
    juce::AudioBuffer<float> tmpMono;      // mono tap of input
    juce::AudioBuffer<float> tmpMonoOut;   // shifter output mono buffer
    juce::AudioBuffer<float> tmpWetStereo; // wet stereo for EMU/ZPlane processing only

    // Smoothing
    juce::SmoothedValue<float> styleSmoothed;
    juce::SmoothedValue<float> strengthSmoothed;
    juce::SmoothedValue<float> retuneSmoothed;
    juce::SmoothedValue<float> mixSmoothed;
    juce::SmoothedValue<float> outputSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> bypassXfade { 0.0f }; // 10 ms

    // A/B State storage
    juce::ValueTree stateA;
    juce::ValueTree stateB;

    // Pitch tracking state (was static, now member for thread safety)
    float lastMidi = 60.0f;
    float heldMidi = 60.0f;
    int holdSamp = 0;

    // Pre-allocated buffers for audio thread (avoid heap allocation)
    std::vector<float> ratioBuf;
    std::vector<float> weightBuf;
    std::vector<float> limitedRatio; // semitone-limited ratios for shifter

    // Helpers
    void updateSnapperScaleFromParams();
    static float midiFromHz (float f0Hz, float lastMidi);

    // RT-safety helper - fail fast if buffer exceeds pre-allocated capacity
    inline void assertCapacity(int numSamples) const noexcept {
        jassert(numSamples <= blockSize); // Debug builds catch capacity violations
        (void)numSamples; // Suppress unused variable warning in release
    }

    // Processing engine methods
    void processVoxZPlane(juce::AudioBuffer<float>& buffer,
                         juce::AudioParameterFloat* pBypass,
                         juce::AudioParameterFloat* pQual,
                         juce::AudioParameterFloat* pStyle,
                         juce::AudioParameterFloat* pRetuneMs);
    void processABTest(juce::AudioBuffer<float>& buffer,
                      juce::AudioParameterFloat* pBypass,
                      juce::AudioParameterFloat* pQual,
                      juce::AudioParameterFloat* pStyle,
                      juce::AudioParameterFloat* pRetuneMs);

    // A/B State management helpers
    void saveCurrentState(juce::ValueTree& targetState);
    void recallState(const juce::ValueTree& sourceState);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchEngineAudioProcessor)
};