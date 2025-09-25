#pragma once
#include <JuceHeader.h>
#include <array>
#include "ForgeVoice.h"
#include "Core/Commands.h"

//==============================================================================
// Manages eight voices, sample loading, and host-sync parameters
class ForgeProcessor
{
public:
    ForgeProcessor();
    ~ForgeProcessor();

    // lifecycle
    void prepareToPlay(double sampleRate, int samplesPerBlock);
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&);

    // commands
    void loadSampleIntoSlot(int slotIdx, const juce::File& file);
    ForgeVoice& getVoice(int index);
    void        setHostBPM(double bpm);
    
    // SAFETY: Preparation guard
    bool prepared() const { return isPrepared.load(std::memory_order_acquire); }

private:
    std::array<ForgeVoice, 8> voices;           // fixed-size, copy-safe
    juce::AudioFormatManager  formatManager;
    float                     hostBPM = 120.0f;
    std::atomic<bool>         isPrepared{false}; // SAFETY: Track initialization state

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ForgeProcessor)
};
