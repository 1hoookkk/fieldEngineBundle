#include "Core/ForgeProcessor.h"
#include "Core/SpectralMask.h"

//==============================================================================
ForgeProcessor::ForgeProcessor()
{
    formatManager.registerBasicFormats();
    // voices[] already default-constructed in std::array – no push_back / clear
}

ForgeProcessor::~ForgeProcessor() = default;

//------------------------------------------------------------------------------
void ForgeProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    for (auto& v : voices)
        v.prepare(sampleRate, samplesPerBlock);
    
    // SAFETY: Mark engine as properly initialized
    isPrepared.store(true, std::memory_order_release);
}

//------------------------------------------------------------------------------
void ForgeProcessor::processBlock(juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer&)
{
    if (!prepared()) return;  // SAFETY: Guard against pre-init calls
    
    juce::ScopedNoDenormals noDenormals;
    for (auto& v : voices)
        v.process(buffer, 0, buffer.getNumSamples());
}

//------------------------------------------------------------------------------
void ForgeProcessor::loadSampleIntoSlot(int slotIdx, const juce::File& file)
{
    if (slotIdx < 0 || slotIdx >= (int)voices.size() || !file.existsAsFile())
        return;

    if (auto* r = formatManager.createReaderFor(file))
    {
        juce::AudioBuffer<float> tmp((int)r->numChannels,
            (int)r->lengthInSamples);
        r->read(&tmp, 0, tmp.getNumSamples(), 0, true, true);
        voices[(size_t)slotIdx].setSample(std::move(tmp), 120.0);
        
        // AUDIO FIX: Auto-start playback for immediate beatmaker feedback
        voices[(size_t)slotIdx].start();
        
        delete r;
    }
    else
    {
        // RT-SAFE: Removed logging from non-audio thread file loading
    }
}

//------------------------------------------------------------------------------
ForgeVoice& ForgeProcessor::getVoice(int index)
{
    // RELIABILITY FIX: Safe bounds checking without blocking audio thread
    if (index < 0 || index >= static_cast<int>(voices.size()))
    {
        DBG("ForgeProcessor: Voice index " << index << " out of bounds, returning voice 0");
        index = 0; // Safe fallback to first voice
    }
    return voices[static_cast<size_t>(index)];
}

//------------------------------------------------------------------------------
void ForgeProcessor::setHostBPM(double bpm)
{
    hostBPM = (float)bpm;
    for (auto& v : voices)
        v.setHostBPM(bpm);
}
