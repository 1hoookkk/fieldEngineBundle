#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace IDs {
    static constexpr auto amount = "amount";
    static constexpr auto output = "output";
    static constexpr auto speed = "speed";
    static constexpr auto depth = "depth";
}

SecretSauceAudioProcessor::SecretSauceAudioProcessor()
: juce::AudioProcessor(BusesProperties()
    .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
    .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
  apvts(*this, nullptr, "PARAMS", createLayout())
{
    amountSmoothed.reset(48000.0, 0.02);  // Responsive but smooth
    outSmoothed.reset(48000.0, 0.01);   // Quick output response
    speedSmoothed.reset(48000.0, 0.03);  // Satisfying speed transitions
    depthSmoothed.reset(48000.0, 0.015); // Buttery smooth depth
}

void SecretSauceAudioProcessor::prepareToPlay (double sr, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);
    amountSmoothed.reset(sr, 0.02);  // Responsive but smooth
    outSmoothed.reset(sr, 0.01);   // Quick output response
    speedSmoothed.reset(sr, 0.03);  // Satisfying speed transitions
    depthSmoothed.reset(sr, 0.015); // Buttery smooth depth
    engine.prepare((float) sr, 512);
}

bool SecretSauceAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (in == juce::AudioChannelSet::disabled() || out == juce::AudioChannelSet::disabled()) return false;
    if (in != out) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void SecretSauceAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused(midi);
    juce::ScopedNoDenormals _;
    const int numSamples = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();

    // Pull params with smooth interpolation for zipper-free knobs
    const float amount = apvts.getRawParameterValue(IDs::amount)->load();
    const float out    = apvts.getRawParameterValue(IDs::output)->load();
    const float speed  = apvts.getRawParameterValue(IDs::speed)->load();
    const float depth  = apvts.getRawParameterValue(IDs::depth)->load();

    amountSmoothed.setTargetValue(amount);
    outSmoothed.setTargetValue(out);
    speedSmoothed.setTargetValue(speed);
    depthSmoothed.setTargetValue(depth);

    engine.setAmount(amountSmoothed.getNextValue());
    engine.setSpeed(speedSmoothed.getNextValue());
    engine.setDepth(depthSmoothed.getNextValue());

    // Process in-place
    float* L = buffer.getWritePointer(0);
    float* R = numCh > 1 ? buffer.getWritePointer(1) : buffer.getWritePointer(0);
    engine.processStereo(L, R, numSamples);

    // Gentle output gain and safety
    buffer.applyGain(outSmoothed.getNextValue());
    for (int ch = 0; ch < numCh; ++ch)
    {
        auto* d = buffer.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i)
        {
            if (!std::isfinite(d[i])) d[i] = 0.0f;
            d[i] = juce::jlimit(-1.0f, 1.0f, d[i]);
        }
    }
}

juce::AudioProcessorEditor* SecretSauceAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}

void SecretSauceAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, dest);
}

void SecretSauceAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorValueTreeState::ParameterLayout SecretSauceAudioProcessor::createLayout()
{
    using P = juce::AudioProcessorValueTreeState;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> ps;

    ps.emplace_back(std::make_unique<juce::AudioParameterFloat>(
        IDs::amount, "Amount", juce::NormalisableRange<float>(0.f, 1.f), 0.7f));
    ps.emplace_back(std::make_unique<juce::AudioParameterFloat>(
        IDs::output, "Output", juce::NormalisableRange<float>(0.0f, 2.0f, 0.0f, 0.4f), 1.0f));

    // Speed: Satisfying increments with musical values (0.01Hz steps)
    ps.emplace_back(std::make_unique<juce::AudioParameterFloat>(
        IDs::speed, "Speed", juce::NormalisableRange<float>(0.1f, 8.0f, 0.01f, 0.3f), 0.5f));

    // Depth: Fine 0.5% increments for precise control
    ps.emplace_back(std::make_unique<juce::AudioParameterFloat>(
        IDs::depth, "Depth", juce::NormalisableRange<float>(0.0f, 1.0f, 0.005f), 0.0f));

    return { ps.begin(), ps.end() };
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SecretSauceAudioProcessor();
}