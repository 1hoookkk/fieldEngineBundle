#include "PluginProcessor.h"
#include "PluginEditor.h"

MorphEngineProcessor::MorphEngineProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

void MorphEngineProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Prepare EMU engine
    emuEngine.prepareToPlay(sampleRate);
    emuEngine.setAutoMakeup(true);

    // Setup smoothing (50ms ramps)
    const double smoothTime = 0.05;
    morphSmoothed.reset(sampleRate, smoothTime);
    resonanceSmoothed.reset(sampleRate, smoothTime);
    brightnessSmoothed.reset(sampleRate, smoothTime);
    driveSmoothed.reset(sampleRate, smoothTime);
    hardnessSmoothed.reset(sampleRate, smoothTime);
    mixSmoothed.reset(sampleRate, smoothTime);

    // Prepare dry buffer
    dryBuffer.setSize(2, samplesPerBlock);


    // Set default style preset
    updateStylePreset(currentStyle);
}

void MorphEngineProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // Get parameters
    auto style = (int)apvts.getRawParameterValue("style")->load();
    auto quality = (int)apvts.getRawParameterValue("quality")->load();

    // Update style preset if changed
    if (style != currentStyle) {
        currentStyle = style;
        updateStylePreset(currentStyle);
    }

    // Update mode
    isTrackMode = (quality == 0);

    // Get smoothed parameter values
    auto morph = apvts.getRawParameterValue("morph")->load();
    auto resonance = apvts.getRawParameterValue("resonance")->load();
    auto brightness = apvts.getRawParameterValue("brightness")->load();
    auto drive = apvts.getRawParameterValue("drive")->load();
    auto hardness = apvts.getRawParameterValue("hardness")->load();
    auto mix = apvts.getRawParameterValue("mix")->load();

    // Update smoothing targets
    morphSmoothed.setTargetValue(morph);
    resonanceSmoothed.setTargetValue(resonance);
    brightnessSmoothed.setTargetValue(brightness);
    driveSmoothed.setTargetValue(drive);
    hardnessSmoothed.setTargetValue(hardness);
    mixSmoothed.setTargetValue(mix);

    // Store dry signal for mix
    dryBuffer.makeCopyOf(buffer);

    // Set Z-plane parameters from UI controls
    emuEngine.setMorphPosition(morphSmoothed.getNextValue() / 100.0f);
    emuEngine.setIntensity(resonanceSmoothed.getNextValue() / 100.0f);
    emuEngine.setDrive((driveSmoothed.getNextValue() - 50.0f) / 50.0f * 9.0f);
    emuEngine.setSectionSaturation(hardnessSmoothed.getNextValue() / 100.0f);

    // Process the audio through the EMU engine
    emuEngine.process(buffer);

    // No extra secret engine: voicing (sat/makeup) is inside emuEngine

    // Apply mix (both Track and Print modes)
    const int numSamples = buffer.getNumSamples();
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
        auto* wet = buffer.getWritePointer(ch);
        auto* dry = dryBuffer.getReadPointer(ch);

        for (int i = 0; i < numSamples; ++i) {
            // Equal-power crossfade: 0=dry, 1=wet
            const float mix01 = mixSmoothed.getNextValue() * 0.01f;
            const float wetGain = std::sin(mix01 * juce::MathConstants<float>::halfPi);
            const float dryGain = std::cos(mix01 * juce::MathConstants<float>::halfPi);
            wet[i] = dryGain * dry[i] + wetGain * wet[i];
        }
    }
}


void MorphEngineProcessor::updateStylePreset(int styleIndex)
{
    // Map Air/Velvet/Focus styles to real morph pairs as tutor suggested
    switch (styleIndex) {
        case 0: // Air
            // Light, airy style - use fewer sections
            emuEngine.setMorphPair(0);  // vowel_pair
            emuEngine.setIntensity(0.3f);  // Light intensity
            break;
        case 1: // Velvet
            // Full, rich style - use more sections
            emuEngine.setMorphPair(1);  // bell_pair
            emuEngine.setIntensity(0.6f);  // Rich intensity
            break;
        case 2: // Focus
            // Precise, focused style
            emuEngine.setMorphPair(2);  // low_pair
            emuEngine.setIntensity(0.5f);  // Balanced intensity
            break;
        default:
            styleIndex = 1; // Default to Velvet
            emuEngine.setMorphPair(1);
            emuEngine.setIntensity(0.6f);
            break;
    }
}

bool MorphEngineProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // Stereo only
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

juce::AudioProcessorEditor* MorphEngineProcessor::createEditor()
{
    return new MorphEngineEditor(*this);
}

void MorphEngineProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void MorphEngineProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState == nullptr)
        return;

    auto vt = juce::ValueTree::fromXml(*xmlState);
    if (! vt.isValid())
        return;

    // Be tolerant to tag/type name changes; copy properties into the expected root type
    juce::ValueTree newState { apvts.state.getType() };
    newState.copyPropertiesAndChildrenFrom(vt, nullptr);

    apvts.replaceState(newState);
}

juce::AudioProcessorValueTreeState::ParameterLayout MorphEngineProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Parameter map as specified by tutor
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        "style", "Style", juce::StringArray{"Air", "Velvet", "Focus"}, 1));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "morph", "Morph", 0.0f, 100.0f, 50.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "resonance", "Resonance", 0.0f, 100.0f, 25.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "brightness", "Brightness", 0.0f, 100.0f, 50.0f));  // 50% = 0 dB/oct

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "drive", "Drive", 0.0f, 100.0f, 50.0f));  // 50% = 0 dB

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "hardness", "Hardness", 0.0f, 100.0f, 0.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "mix", "Mix", 0.0f, 100.0f, 75.0f));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        "quality", "Quality", juce::StringArray{"Track", "Print"}, 0));

    return layout;
}

// Plugin creation
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MorphEngineProcessor();
}
