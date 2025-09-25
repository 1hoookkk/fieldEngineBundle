#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "CoreMinimal/Types.h"
#include "DSPMinimal/STFTProcessor.h"
#include "DSPMinimal/MaskGrid.h"

class SpectralCanvasProMinimalProcessor : public juce::AudioProcessor {
public:
    SpectralCanvasProMinimalProcessor();
    ~SpectralCanvasProMinimalProcessor() override = default;

    // AudioProcessor
    void prepareToPlay (double sampleRate, int maxBlock) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;

    const juce::String getName() const override { return "SpectralCanvasProMinimal"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

    // UI accessors (thread-safe)
    SPSCQueue<BrushCommand,4096>& brushQueue() noexcept { return brushQ_; }
    std::vector<float> magnitudesForUI() const noexcept { return stft_.getMagnitudesForUI(); }
    MaskSnapshot maskSnapshotForUI() const noexcept { return mask_.snapshotPrevious(); }

private:
    STFTProcessor stft_;
    MaskGrid mask_;
    SPSCQueue<BrushCommand,4096> brushQ_;
    STFTConfig cfg_{ 2048, 512 };
};