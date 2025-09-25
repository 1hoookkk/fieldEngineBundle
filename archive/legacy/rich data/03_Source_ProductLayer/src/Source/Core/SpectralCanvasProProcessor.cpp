#include "SpectralCanvasProProcessor.h"
#include "SpectralCanvasProEditor.h"

SpectralCanvasProProcessor::SpectralCanvasProProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

SpectralCanvasProProcessor::~SpectralCanvasProProcessor()
{
}

void SpectralCanvasProProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
}

void SpectralCanvasProProcessor::releaseResources()
{
}

void SpectralCanvasProProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
}

juce::AudioProcessorEditor* SpectralCanvasProProcessor::createEditor()
{
    return new SpectralCanvasProEditor(*this);
}

// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SpectralCanvasProProcessor();
}
