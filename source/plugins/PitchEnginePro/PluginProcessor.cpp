#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr float kStyleScale = 0.01f;
constexpr float kStyleSmoothSeconds = 0.05f;
constexpr float kStrengthSmoothSeconds = 0.10f;
constexpr float kRetuneSmoothSeconds = 0.20f;
constexpr float kBypassFadeSeconds = 0.005f;

inline float loadParam(std::atomic<float>* param, float fallback = 0.0f)
{
    return param != nullptr ? param->load() : fallback;
}
} // namespace

PitchEngineAudioProcessor::PitchEngineAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    cacheParameterPointers();
    bypassAmount.setCurrentAndTargetValue(0.0f);
}

bool PitchEngineAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getChannelSet (true,  0);
    const auto& out = layouts.getChannelSet (false, 0);
    return (in == out) && (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}

void PitchEngineAudioProcessor::cacheParameterPointers()
{
    cachedParams.style    = apvts.getRawParameterValue ("style");
    cachedParams.strength = apvts.getRawParameterValue ("strength");
    cachedParams.retune   = apvts.getRawParameterValue ("retuneMs");
    cachedParams.bypass   = apvts.getRawParameterValue ("bypass");
    cachedParams.secret   = apvts.getRawParameterValue ("secretMode");
}

void PitchEngineAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);

    cacheParameterPointers();

    zplane.prepare (sampleRate);

    styleSmoothed.reset (sampleRate, kStyleSmoothSeconds);
    strengthSmoothed.reset (sampleRate, kStrengthSmoothSeconds);
    retuneSmoothed.reset (sampleRate, kRetuneSmoothSeconds);

    styleSmoothed.setCurrentAndTargetValue (loadParam (cachedParams.style, 35.0f));
    strengthSmoothed.setCurrentAndTargetValue (loadParam (cachedParams.strength, 100.0f));
    retuneSmoothed.setCurrentAndTargetValue (loadParam (cachedParams.retune, 12.0f));

    bypassAmount.reset (sampleRate, kBypassFadeSeconds);
    const bool bypassed = loadParam (cachedParams.bypass) > 0.5f;
    bypassAmount.setCurrentAndTargetValue (bypassed ? 1.0f : 0.0f);

    const int requiredSamples = juce::jmax (samplesPerBlock, 1);
    dryBypassBuffer.setSize ((int) getTotalNumOutputChannels(), requiredSamples, false, false, true);
    dryBypassBuffer.clear();
}

void PitchEngineAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    if (numSamples == 0)
        return;

    if (dryBypassBuffer.getNumChannels() != numChannels || dryBypassBuffer.getNumSamples() < numSamples)
        dryBypassBuffer.setSize (numChannels, juce::jmax (numSamples, 1), false, false, true);

    for (int ch = 0; ch < numChannels; ++ch)
        dryBypassBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);

    if (cachedParams.style == nullptr)
        cacheParameterPointers();

    const bool bypassRequested = loadParam (cachedParams.bypass) > 0.5f;
    bypassAmount.setTargetValue (bypassRequested ? 1.0f : 0.0f);

    const bool shouldProcess = bypassAmount.getTargetValue() < 0.999f
                             || bypassAmount.getCurrentValue() < 0.999f;

    if (shouldProcess)
    {
        const float styleRaw    = juce::jlimit (0.0f, 100.0f, loadParam (cachedParams.style, 35.0f));
        const float strengthRaw = juce::jlimit (0.0f, 100.0f, loadParam (cachedParams.strength, 100.0f));
        const float retuneRaw   = juce::jlimit (1.0f, 200.0f, loadParam (cachedParams.retune, 12.0f));
        const bool secret       = loadParam (cachedParams.secret) > 0.5f;

        styleSmoothed.setTargetValue (styleRaw);
        strengthSmoothed.setTargetValue (strengthRaw);
        retuneSmoothed.setTargetValue (retuneRaw);

        const float style = styleSmoothed.getNextValue() * kStyleScale;
        const float strength = strengthSmoothed.getNextValue();
        const float retune = retuneSmoothed.getNextValue();
        juce::ignoreUnused (strength, retune);

        zplane.setSecretMode (secret);
        zplane.process (buffer, style);
    }
    else
    {
        for (int ch = 0; ch < numChannels; ++ch)
            buffer.copyFrom (ch, 0, dryBypassBuffer, ch, 0, numSamples);

        bypassAmount.setCurrentAndTargetValue (1.0f);
        return;
    }

    if (! bypassAmount.isSmoothing() && bypassAmount.getCurrentValue() <= 1.0e-4f)
        return;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float mix = bypassAmount.getNextValue();
        const float wetGain = 1.0f - mix;
        const float dryGain = mix;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* wet = buffer.getWritePointer (ch);
            const auto* dry = dryBypassBuffer.getReadPointer (ch);
            wet[sample] = wet[sample] * wetGain + dry[sample] * dryGain;
        }
    }
}

void PitchEngineAudioProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    if (numSamples == 0)
        return;

    if (dryBypassBuffer.getNumChannels() != numChannels || dryBypassBuffer.getNumSamples() < numSamples)
        dryBypassBuffer.setSize (numChannels, juce::jmax (numSamples, 1), false, false, true);

    for (int ch = 0; ch < numChannels; ++ch)
        dryBypassBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);

    bypassAmount.setCurrentAndTargetValue (1.0f);
}

void PitchEngineAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void PitchEngineAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
        cacheParameterPointers();
    }
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

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PitchEngineAudioProcessor();
}
