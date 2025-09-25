#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../shared/MorphFilter.h"
#include "../shared/EMUFilterModels.h"
#include "../shared/EMUFilter.h"
#include "../shared/AtomicOscillator.h"
#include <atomic>
#include <array>
#include "../ui/UiStateProvider.h"
#include "../shared/ZPlaneFilter.h"
#include "../shared/preset_loader_json.hpp"
#include <map>
#include "../dsp/MorphEngine.h"

class FieldEngineFXProcessor : public juce::AudioProcessor,
                               public UiStateProvider
{
public:
    FieldEngineFXProcessor();
    ~FieldEngineFXProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "FieldEngineFX"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() { return parameters; }

    // UiStateProvider implementation
    double getSampleRate() const override { return currentSampleRate.load(); }
    float  getMasterMorphAlpha() const override { return masterAlpha.load(); }
    bool   isBypassed() const override { return bypass.load(); }
    bool   isSidechainActive() const override { return sidechain.load(); }
    juce::String getTitle() const override { return "fieldEngine"; }

    int getNumBands() const override { return kNumBands; }
    juce::String getBandName (int b) const override { return bandNames[b]; }
    float getBandEnergy (int b) const override { return bandEnergy[b].load(); }
    float getBandMorphAlpha (int b) const override { return bandAlpha[b].load(); }
    float getBandGainDb (int b) const override { return bandGainDb[b].load(); }
    bool  isBandMuted (int b) const override { return bandMuted[b].load(); }
    juce::String getBandMorphPath (int b) const override { return bandPath[b]; }

    // Telemetry access for UI (lock-free)
    void drainTelemetry(float* dst, int maxToRead, int& numRead);
    void getMorphTelemetry(float& rmsL, float& rmsR, float& peakL, float& peakR, float& morphX, float& morphY, bool& clipped) const;

private:
    juce::AudioProcessorValueTreeState parameters;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    MorphFilter morphFilter;
    std::array<AuthenticEMUZPlane, 2> channelFilters{};
    std::array<EMUFilterModelProcessor, 2> emuFilterModels{};
    fe::ZPlaneFilter zFilter; // New Z-plane morphing engine

    // AUTHENTIC EMU Z-PLANE MORPHING with real coefficients
    AuthenticEMUZPlane authenticEMU;

    AtomicOscillator lfo;
    float lfoPhase = 0.0f;

    // ENHANCED MODULATION SOURCES
    struct EnvelopeFollower {
        float value = 0.0f;
        float attack = 0.000489f;  // Default from Planet Phatt
        float release = 0.08f;     // EMU default
        float sampleRate = 48000.0f;

        void setSampleRate(double sr) { sampleRate = (float)sr; }
        void setAttack(float seconds) { attack = juce::jlimit(0.0001f, 2.0f, seconds); }
        void setRelease(float seconds) { release = juce::jlimit(0.001f, 5.0f, seconds); }

        float process(float input) {
            float targetLevel = std::abs(input);
            float rate = (targetLevel > value) ?
                        (1.0f - std::exp(-1.0f / (attack * sampleRate))) :
                        (1.0f - std::exp(-1.0f / (release * sampleRate)));
            value += (targetLevel - value) * rate;
            return value;
        }

        void reset() { value = 0.0f; }
    } envelopeFollower;

    // Multi-shape LFO
    enum LFOShape { Sine = 0, Triangle, Square, Saw };
    float generateLFOSample(LFOShape shape, float phase);
    LFOShape currentLFOShape = Sine;
    
    // Smoothed parameters for click-free audio
    juce::LinearSmoothedValue<float> morphSmoother;
    juce::LinearSmoothedValue<float> intensitySmoother;
    juce::LinearSmoothedValue<float> driveSmoother;
    juce::LinearSmoothedValue<float> outputSmoother;
    juce::LinearSmoothedValue<float> mixSmoother;
    juce::LinearSmoothedValue<float> lfoRateSmoother;
    juce::LinearSmoothedValue<float> lfoAmountSmoother;

    // UI-visible state (atomic for lock-free cross-thread reads)
    std::atomic<double> currentSampleRate { 48000.0 };
    std::atomic<float>  masterAlpha { 0.0f };
    std::atomic<bool>   bypass { false };
    std::atomic<bool>   sidechain { false };

    static constexpr int kNumBands = 8;
    juce::StringArray bandNames { "SUB","LOW","LOWMID","MID","UPMID","HI","AIR","SPARK" };
    std::array<std::atomic<float>, kNumBands> bandEnergy {};
    std::array<std::atomic<float>, kNumBands> bandAlpha {};
    std::array<std::atomic<float>, kNumBands> bandGainDb {};
    std::array<std::atomic<bool>,  kNumBands> bandMuted {};
    std::array<juce::String,       kNumBands> bandPath { "LP→BP","BP→HP","HP→LP","LP→NT","NT→BP","BP→PH","PH→HP","LP→COMB" };

    // RT-SAFE: Authentic shape maps loaded in background thread
    std::map<juce::String, std::array<fe::PolePair, fe::ZPLANE_N_SECTIONS>> shapeMapA48k;
    std::map<juce::String, std::array<fe::PolePair, fe::ZPLANE_N_SECTIONS>> shapeMapB48k;
    std::atomic<bool> shapesLoaded { false };
    int lastPairIndex = -1; // 0=vowel, 1=bell, 2=low
    
    void loadEMUShapeDataAsync();
    bool applyPairByIndex(int index);

    // Neutral morph engine for UI mapping + telemetry
    fe::MorphEngine morphEngine;

    // Lock-free telemetry buffer (mono)
    juce::AbstractFifo telemetryFifo { 8192 };
    std::vector<float> telemetryRing;

    // Telemetry snapshot (atomics)
    std::atomic<float> teleRmsL { 0.0f }, teleRmsR { 0.0f };
    std::atomic<float> telePeakL { 0.0f }, telePeakR { 0.0f };
    std::atomic<float> teleMorphX { 0.0f }, teleMorphY { 0.5f };
    std::atomic<int>   teleClipped { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FieldEngineFXProcessor)
};
