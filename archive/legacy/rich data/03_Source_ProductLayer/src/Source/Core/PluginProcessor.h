// Source/PluginProcessor.h
#pragma once

#include <JuceHeader.h>
#include "Core/Commands.h"
#include "Core/CommandQueue.h"
#include "Core/ForgeProcessor.h"
#include "Core/PaintEngine.h"
#include "Core/SampleMaskingEngine.h"
#include "Core/ParameterBridge.h"
#include "Core/AudioRecorder.h"
#include "Core/SpectralSynthEngine.h"
#include "Core/SpectralSynthEngineStub.h"
#include "Core/PaintQueue.h"
#include "Core/EMUFilter.h"
#include "Core/TubeStage.h"
#include "Core/RTMetrics.h"

class ARTEFACTAudioProcessor : public juce::AudioProcessor,
    public juce::AudioProcessorValueTreeState::Listener
{
public:
    ARTEFACTAudioProcessor();
    ~ARTEFACTAudioProcessor() override;

    void prepareToPlay(double, int) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "SpectralCanvas Pro"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    bool pushCommandToQueue(const Command& newCommand);

    void parameterChanged(const juce::String&, float) override;
    
    // Accessors for GUI
    ForgeProcessor& getForgeProcessor() { return forgeProcessor; }
    PaintEngine& getPaintEngine() { return paintEngine; }
    SampleMaskingEngine& getSampleMaskingEngine() { return sampleMaskingEngine; }
    SpectralSynthEngine& getSpectralSynthEngine() { return SpectralSynthEngine::instance(); }
    SpectralSynthEngineStub* getSpectralSynthEngineStub() { return &spectralSynthEngineStub; }
    AudioRecorder& getAudioRecorder() { return audioRecorder; }
    
    // Always-on character chain accessors
    EMUFilter& getEMUFilter() { return emuFilter; }
    TubeStage& getTubeStage() { return tubeStage; }
    
    // Paint queue interface for Y2K theme
    SpectralPaintQueue* getPaintQueue() { return &paintQueue; }
    
    // Paint Brush System
    void setActivePaintBrush(int slotIndex);
    int getActivePaintBrush() const { return activePaintBrushSlot; }
    void triggerPaintBrush(float canvasY, float pressure = 1.0f);
    void stopPaintBrush();
    
    // Audio Processing Control (prevents feedback when minimized)
    void pauseAudioProcessing();
    void resumeAudioProcessing();
    bool isAudioProcessingPaused() const { return audioProcessingPaused; }
    
    // BPM Sync
    void setTempo(double bpm) { lastKnownBPM = bpm; }
    double getTempo() const { return lastKnownBPM; }
    
    // Missing methods needed by UI
    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }
    void setMagicSwitch(bool enabled);
    
    // Preview tone control
    void setPreviewEnabled(bool enabled) { previewEnabled.store(enabled ? 1 : 0, std::memory_order_release); }
    bool isPreviewEnabled() const { return previewEnabled.load(std::memory_order_acquire) != 0; }
    // Canonical stroke event handler
    void processStrokeEvent(const StrokeEvent& e);
    
    // Legacy shims for existing calls
    void processStrokeEvent(float x, float y, float pressure, juce::Colour color) {
        StrokeEvent e; e.x=(int)x; e.y=(int)y; e.pressure=pressure; e.colour=color; 
        processStrokeEvent(e);
    }
    
    // Paint queue interface for real-time paint-to-audio
    bool pushPaintEvent(const PaintEvent& event) { return paintQueue.push(event); }
    bool pushPaintEvent(float x, float y, float pressure, uint32_t flags = kStrokeMove) {
        return paintQueue.push(PaintEvent(x, y, pressure, flags));
    }
    
    // State flags
    std::atomic<bool> editorOpen{false};
    std::atomic<bool> enginePrepared{false};

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState apvts;

    ForgeProcessor  forgeProcessor;
    PaintEngine paintEngine;
    SampleMaskingEngine sampleMaskingEngine;
    // SpectralSynthEngine is a singleton, accessed via instance()
    SpectralSynthEngineStub spectralSynthEngineStub;
    ParameterBridge parameterBridge;
    AudioRecorder audioRecorder;
    
    // Always-on character chain: EMU → Spectral → Tube
    EMUFilter emuFilter;
    TubeStage tubeStage;

    enum class ProcessingMode { Forge = 0, Canvas, Hybrid };
    ProcessingMode currentMode = ProcessingMode::Canvas;

    // Thread-safe command queue
    CommandQueue<512> commandQueue;  // Increased size for better performance
    
    // Paint event queue for real-time paint-to-audio
    SpectralPaintQueue paintQueue;
    
    // Command processing methods
    void processCommands();
    void processCommand(const Command& cmd);
    void processForgeCommand(const Command& cmd);
    void processSampleMaskingCommand(const Command& cmd);
    void processPaintCommand(const Command& cmd);
    void processRecordingCommand(const Command& cmd);

    double lastKnownBPM = 120.0;
    double currentSampleRate = 44100.0;
    
    // Paint brush system
    int activePaintBrushSlot = 0;  // Which ForgeVoice slot is the active brush (0-7)
    
    // Audio processing control
    bool audioProcessingPaused = false;
    
    // RT-safe test tone and stroke-to-audio bridge
    float previewPhase = 0.0f;
    std::atomic<int> previewEnabled{0};
    std::atomic<int> hardMute{0};
    std::atomic<float> masterGain{0.7f};
    std::atomic<float> currentFrequency{440.0f}; // Debug: maps paint Y to sine frequency
    
    // Startup ping to prove audio device is working (can't lie)
    double startupPhase = 0.0;
    int warmupSamples = 0;
    
    struct BrushFrame {
        float pressure = 0.0f;
        float size = 0.0f; 
        float speed = 0.0f;
    };
    BrushFrame uiFrame;
    std::atomic<int> frameDirty{0};

    // Preallocated temp buffers to avoid per-block allocations
    juce::AudioBuffer<float> preallocMasking;
    juce::AudioBuffer<float> preallocPaint;
    int preallocChannels = 0;
    int preallocBlockSize = 0;
    
    // RT-safe metrics collection for subagent monitoring
    std::unique_ptr<RTMetricsReporter> metricsReporter;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ARTEFACTAudioProcessor)
};
