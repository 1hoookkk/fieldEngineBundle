#include "PluginProcessor.h"
#include "PluginEditor.h"

PitchEngineAudioProcessor::PitchEngineAudioProcessor()
: juce::AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

bool PitchEngineAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getChannelSet (true,  0);
    const auto& out = layouts.getChannelSet (false, 0);
    return (in == out) && (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}

void PitchEngineAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    zplane.prepare (sampleRate);

    // Initialize parameter smoothing for premium feel
    styleSmoothed.reset (sampleRate, 0.05);    // 50ms smooth
    strengthSmoothed.reset (sampleRate, 0.1);  // 100ms smooth
    retuneSmoothed.reset (sampleRate, 0.2);    // 200ms smooth
}

void PitchEngineAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals _;

    auto* styleParam     = apvts.getRawParameterValue ("style");
    auto* strengthParam  = apvts.getRawParameterValue ("strength");
    auto* retuneParam    = apvts.getRawParameterValue ("retuneMs");
    auto* bypassParam    = apvts.getRawParameterValue ("bypass");
    auto* secretParam    = apvts.getRawParameterValue ("secretMode");

    if (*bypassParam > 0.5f) return; // soft bypass (no crossfade for brevity)

    // Set secret mode on the Z-plane engine
    const bool secret = (*secretParam > 0.5f);
    zplane.setSecretMode (secret);

    // Update smoothed values for premium feel
    styleSmoothed.setTargetValue (juce::jlimit (0.0f, 100.0f, styleParam->load()));
    strengthSmoothed.setTargetValue (juce::jlimit (0.0f, 100.0f, strengthParam->load()));
    retuneSmoothed.setTargetValue (juce::jlimit (1.0f, 200.0f, retuneParam->load()));

    // Get smoothed style value for this block
    const float style = styleSmoothed.getNextValue() * 0.01f;

    // Apply Style processing to the entire buffer
    zplane.process (buffer, style);
}

void PitchEngineAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void PitchEngineAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorValueTreeState::ParameterLayout PitchEngineAudioProcessor::createLayout()
{
    using AP = juce::AudioProcessorValueTreeState;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    p.push_back (std::make_unique<juce::AudioParameterChoice> ("key", "Key",
        juce::StringArray { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" }, 9));
    p.push_back (std::make_unique<juce::AudioParameterChoice> ("scale", "Scale",
        juce::StringArray { "Chromatic","Major","Minor" }, 2));

    p.push_back (std::make_unique<juce::AudioParameterFloat> ("retuneMs", "Retune (ms)",
        juce::NormalisableRange<float> (1.0f, 200.0f, 0.01f), 12.0f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> ("strength", "Strength",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), 100.0f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> ("formant", "Formant",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), 80.0f));

    p.push_back (std::make_unique<juce::AudioParameterFloat> ("style", "Style",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), 35.0f));

    p.push_back (std::make_unique<juce::AudioParameterChoice> ("stabilizer", "Stabilizer",
        juce::StringArray { "Off","Short","Mid","Long" }, 0));

    p.push_back (std::make_unique<juce::AudioParameterChoice> ("qualityMode", "Quality",
        juce::StringArray { "Track","Print" }, 0));

    p.push_back (std::make_unique<juce::AudioParameterBool> ("autoGain", "Auto Gain", true));
    p.push_back (std::make_unique<juce::AudioParameterBool> ("bypass", "Bypass", false));
    p.push_back (std::make_unique<juce::AudioParameterBool> ("secretMode", "Mode X", false));

    return { p.begin(), p.end() };
}

juce::AudioProcessorEditor* PitchEngineAudioProcessor::createEditor()
{
    return new PitchEngineEditor (*this);
}

// Plugin entry point
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PitchEngineAudioProcessor();
}