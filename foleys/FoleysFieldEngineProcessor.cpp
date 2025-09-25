#include "FoleysFieldEngineProcessor.h"

FoleysFieldEngineProcessor::FoleysFieldEngineProcessor()
    : juce::AudioProcessor(BusesProperties()
                            .withInput("Input", juce::AudioChannelSet::stereo(), true)
                            .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    setupMagicState();
}

FoleysFieldEngineProcessor::~FoleysFieldEngineProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout FoleysFieldEngineProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Core morphing parameters
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "morph", "Morph", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "cutoff", "Cutoff", juce::NormalisableRange<float>(20.0f, 20000.0f, 0.0f, 0.25f), 1000.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "resonance", "Resonance", juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "drive", "Drive", juce::NormalisableRange<float>(0.0f, 24.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "output", "Output", juce::NormalisableRange<float>(-24.0f, 24.0f), 0.0f));

    // LFO parameters
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "lfoRate", "LFO Rate", juce::NormalisableRange<float>(0.02f, 8.0f, 0.0f, 0.25f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "lfoDepth", "LFO Depth", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));

    // Modulation
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "envDepth", "Envelope Depth", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));

    // Mix controls
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "dryWet", "Dry/Wet", juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "bypass", "Bypass", false));

    return { params.begin(), params.end() };
}

void FoleysFieldEngineProcessor::setupMagicState()
{
    // Configure Foleys GUI Magic state
    magicState.setApplicationSettingsFile(juce::File::getSpecialLocation(
        juce::File::userApplicationDataDirectory)
        .getChildFile("FieldEngine")
        .getChildFile("settings.xml"));

    // Add custom visualizers and data sources
    magicState.addTrigger("reset", [&] {
        morphFilter.reset();
        for (auto& filter : channelFilters)
            filter.reset();
    });

    // Add spectrum analyzer data source
    magicState.addPlotSource("spectrum", std::make_unique<foleys::MagicAnalyser>(magicState));

    // Add oscilloscope data source
    magicState.addPlotSource("oscilloscope", std::make_unique<foleys::MagicOscilloscope>(magicState));

    // Add level meter data source
    magicState.addLevelSource("input", std::make_unique<foleys::MagicLevelSource>(magicState));
    magicState.addLevelSource("output", std::make_unique<foleys::MagicLevelSource>(magicState));

    // Register custom look and feel
    magicState.setDefaultLookAndFeel(BinaryData::magic_xml, BinaryData::magic_xmlSize);
}

void FoleysFieldEngineProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate.store(sampleRate);

    // Prepare DSP components
    morphFilter.prepare(sampleRate, samplesPerBlock);
    for (auto& filter : channelFilters)
        filter.prepareToPlay(sampleRate);

    lfo.prepare(sampleRate);

    // Prepare smoothers
    morphSmoother.reset(sampleRate, 0.02);
    cutoffSmoother.reset(sampleRate, 0.02);
    resonanceSmoother.reset(sampleRate, 0.02);
    driveSmoother.reset(sampleRate, 0.02);
    outputSmoother.reset(sampleRate, 0.02);

    // Prepare Foleys analysis
    magicState.prepareToPlay(sampleRate, samplesPerBlock);
}

void FoleysFieldEngineProcessor::releaseResources()
{
    // Release resources
}

void FoleysFieldEngineProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();
    const auto numSamples = buffer.getNumSamples();

    // Clear unused output channels
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, numSamples);

    // Update parameters from APVTS
    morphSmoother.setTargetValue(*parameters.getRawParameterValue("morph"));
    cutoffSmoother.setTargetValue(*parameters.getRawParameterValue("cutoff"));
    resonanceSmoother.setTargetValue(*parameters.getRawParameterValue("resonance"));
    driveSmoother.setTargetValue(*parameters.getRawParameterValue("drive"));
    outputSmoother.setTargetValue(juce::Decibels::decibelsToGain(*parameters.getRawParameterValue("output")));
    
    const float lfoRateValue = *parameters.getRawParameterValue("lfoRate");
    const float lfoDepthValue = *parameters.getRawParameterValue("lfoDepth");
    const float dryWetValue = *parameters.getRawParameterValue("dryWet");
    const bool bypassValue = *parameters.getRawParameterValue("bypass");

    // Store values for UI
    masterAlpha.store(morphSmoother.getTargetValue());
    bypass.store(bypassValue);

    if (bypassValue)
    {
        // Bypass processing, just pass through
        magicState.processMidiBuffer(midiMessages, numSamples);
        return;
    }

    // Update LFO
    lfo.setFrequency(lfoRateValue);

    // Process morphing filter
    morphFilter.setMorph(morphSmoother.getNextValue());
    morphFilter.setCutoff(cutoffSmoother.getNextValue());
    morphFilter.setResonance(resonanceSmoother.getNextValue());
    morphFilter.setDrive(driveSmoother.getNextValue());

    morphFilter.process(buffer);

    // Apply output gain
    const float outputGain = outputSmoother.getNextValue();
    buffer.applyGain(outputGain);

    // Update analysis data for Foleys visualizers
    updateAnalysisData();

    // Process Foleys state
    magicState.processMidiBuffer(midiMessages, numSamples);
}

void FoleysFieldEngineProcessor::updateAnalysisData()
{
    // FIXME: This is a placeholder for the analysis data.
    // Replace this with a proper FFT or filter-based analysis of the audio stream.
    // Update band energy for visualizers
    for (int band = 0; band < kNumBands; ++band)
    {
        // Simplified energy calculation - in real implementation,
        // you'd use proper frequency analysis
        bandEnergy[band].store(0.5f + 0.3f * std::sin(juce::Time::getMillisecondCounterHiRes() * 0.001f + band));
        bandAlpha[band].store(masterAlpha.load() * (band + 1) / kNumBands);
        bandGainDb[band].store(-12.0f + band * 3.0f);
        bandMuted[band].store(false);
    }
}

juce::AudioProcessorEditor* FoleysFieldEngineProcessor::createEditor()
{
    auto* editor = new foleys::MagicPluginEditor(magicState, BinaryData::magic_xml, BinaryData::magic_xmlSize);
    return editor;
}

void FoleysFieldEngineProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void FoleysFieldEngineProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
}