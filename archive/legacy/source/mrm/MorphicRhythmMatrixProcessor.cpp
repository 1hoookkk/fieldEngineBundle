#include "MorphicRhythmMatrixProcessor.h"
#include "../ui/TempleEditor.h"

MorphicRhythmMatrixProcessor::MorphicRhythmMatrixProcessor()
: juce::AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

MorphicRhythmMatrixProcessor::~MorphicRhythmMatrixProcessor() = default;

void MorphicRhythmMatrixProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);
    currentSampleRate.store(sampleRate);
    sampleCounter = 0;
}

void MorphicRhythmMatrixProcessor::releaseResources() {}

bool MorphicRhythmMatrixProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    auto in = layouts.getMainInputChannelSet();
    auto out = layouts.getMainOutputChannelSet();
    if (in == juce::AudioChannelSet::disabled() || out == juce::AudioChannelSet::disabled()) return false;
    if (in != out) return false;
    if (in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo()) return false;
    return true;
}

void MorphicRhythmMatrixProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused(midi);
    juce::ScopedNoDenormals _;

    auto* morphParam = parameters.getRawParameterValue("morph");
    const float morph = morphParam ? morphParam->load() : 0.0f;
    masterAlpha.store(morph);

    // simple phase for fake energy animation (~0.7 Hz)
    double sr = currentSampleRate.load();
    double phase = (double)sampleCounter / juce::jmax(1.0, sr) * 0.7 * juce::MathConstants<double>::twoPi;
    sampleCounter += (std::uint64_t) buffer.getNumSamples();

    for (int i = 0; i < kNumBands; ++i)
    {
        float e = bandEnergy[i].load();
        float target = 0.1f + 0.8f * (0.5f * (1.0f + (float)std::sin(phase + 0.35 * (double)i)));
        e = 0.9f * e + 0.1f * target;
        bandEnergy[i].store(juce::jlimit(0.0f, 1.0f, e));

        float a = bandAlpha[i].load();
        a = 0.95f * a + 0.05f * morph;
        bandAlpha[i].store(juce::jlimit(0.0f, 1.0f, a));

        float g = bandGainDb[i].load();
        g = 0.98f * g + 0.02f * juce::jmap(morph, -6.0f, 6.0f);
        bandGainDb[i].store(g);
    }

    juce::ignoreUnused(buffer);
}

juce::AudioProcessorEditor* MorphicRhythmMatrixProcessor::createEditor()
{
    return new TempleEditor (*this);
}

void MorphicRhythmMatrixProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    if (state.isValid())
        if (auto xml = state.createXml())
            copyXmlToBinary(*xml, destData);
}

void MorphicRhythmMatrixProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorValueTreeState::ParameterLayout MorphicRhythmMatrixProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back(std::make_unique<juce::AudioParameterFloat>("morph", "Morph", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("lfoRate", "LFO Rate", juce::NormalisableRange<float>(0.02f, 8.0f, 0.0f, 0.35f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("lfoDepth", "LFO Depth", 0.0f, 1.0f, 0.2f));
    return { params.begin(), params.end() };
}

// JUCE plugin factory
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MorphicRhythmMatrixProcessor();
}


