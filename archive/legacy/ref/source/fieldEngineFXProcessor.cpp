#include "fieldEngineFXProcessor.h"
#include "fieldEngineFXEditor.h"

fieldEngineFXProcessor::fieldEngineFXProcessor()
    : juce::AudioProcessor(BusesProperties()
                          .withInput("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "Parameters", createParameterLayout())
{
    // Initialize filter response array
    filterResponse.fill(0.0f);
}

fieldEngineFXProcessor::~fieldEngineFXProcessor()
{
}

juce::AudioProcessorValueTreeState::ParameterLayout fieldEngineFXProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Morph position (0.0 to 1.0)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "MORPH", "Morph", 0.0f, 1.0f, 0.5f));

    // LFO Rate (0.01 to 20.0 Hz when free running)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "LFO_RATE", "LFO Rate", 0.01f, 20.0f, 1.0f));

    // LFO Amount (0.0 to 1.0)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "LFO_AMOUNT", "LFO Amount", 0.0f, 1.0f, 0.1f));

    // LFO Sync Mode
    juce::StringArray syncChoices = { "Free", "1/4", "1/8", "1/16", "1/32" };
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "LFO_SYNC", "LFO Sync", syncChoices, 0));

    // Drive (input gain before filter)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "DRIVE", "Drive", 0.1f, 4.0f, 1.0f));

    // Output Level
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "OUTPUT", "Output", 0.0f, 2.0f, 1.0f));

    return { params.begin(), params.end() };
}

void fieldEngineFXProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    
    // Initialize LFO
    lfo.setSampleRate(static_cast<float>(sampleRate));
    lfo.setFrequency(1.0f); // Default 1Hz
    
    // Initialize filters for stereo processing
    morphFilters.clear();
    emuFilters.clear();
    
    for (int channel = 0; channel < 2; ++channel)
    {
        // Create MorphFilter with correct prepare signature
        auto morphFilter = std::make_unique<MorphFilter>();
        morphFilter->prepare(sampleRate, samplesPerBlock);
        morphFilters.push_back(std::move(morphFilter));
        
        // Create EMUFilterCore with correct prepare signature
        auto emuFilter = std::make_unique<EMUFilterCore>();
        emuFilter->prepareToPlay(sampleRate);
        emuFilters.push_back(std::move(emuFilter));
    }
    
    updateLFOFrequency();
}

void fieldEngineFXProcessor::releaseResources()
{
    morphFilters.clear();
    emuFilters.clear();
}

bool fieldEngineFXProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // Support mono and stereo
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono()
        || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void fieldEngineFXProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear any unused output channels
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // Update tempo sync
    updateLFOFrequency();

    // Get parameter values
    float morphBase = parameters.getRawParameterValue("MORPH")->load();
    float lfoAmount = parameters.getRawParameterValue("LFO_AMOUNT")->load();
    float drive = parameters.getRawParameterValue("DRIVE")->load();
    float output = parameters.getRawParameterValue("OUTPUT")->load();

    // Update filter parameters
    for (auto& morphFilter : morphFilters)
    {
        morphFilter->setMorph(morphBase);
        morphFilter->setDrive(drive);
    }

    // Process each sample
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        // Generate LFO sample
        float lfoSample = lfo.generateSample(static_cast<float>(currentSampleRate));
        float lfoMod = lfoSample * lfoAmount;
        float morphPosition = juce::jlimit(0.0f, 1.0f, morphBase + lfoMod);

        // Store current values for visualizer
        currentMorphPosition = morphPosition;
        currentLFOValue = lfoSample;

        // Process each channel
        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            auto* channelData = buffer.getWritePointer(channel);
            float inputSample = channelData[sample];

            // Apply drive
            inputSample *= drive;

            // Update envelope follower for visualizer (first channel only)
            if (channel == 0)
            {
                float rectified = std::abs(inputSample);
                currentEnvelopeValue = currentEnvelopeValue * 0.999f + rectified * 0.001f;
            }

            // Update morph filter parameters
            morphFilters[channel]->setMorph(morphPosition);

            // Process through EMU filter
            float processedSample = emuFilters[channel]->processSample(inputSample);

            // Apply output level
            channelData[sample] = processedSample * output;
        }
    }

    // Process through morph filters using block processing
    for (size_t i = 0; i < morphFilters.size() && i < 2; ++i)
    {
        morphFilters[i]->process(buffer);
    }

    // Update filter response for visualizer (simplified)
    updateFilterResponse();
}

void fieldEngineFXProcessor::updateLFOFrequency()
{
    auto playHead = getPlayHead();
    if (playHead != nullptr)
    {
        auto position = playHead->getPosition();
        if (position.hasValue())
        {
            auto bpm = position->getBpm();
            auto isPlaying = position->getIsPlaying();

            if (bpm.hasValue())
            {
                hostTempo = *bpm;
                this->isPlaying = isPlaying;
            }
        }
    }

    // Get current sync mode
    int syncChoice = static_cast<int>(parameters.getRawParameterValue("LFO_SYNC")->load());
    currentSyncMode = static_cast<SyncMode>(syncChoice);

    if (currentSyncMode == FREE)
    {
        // Use free-running LFO rate
        float rate = parameters.getRawParameterValue("LFO_RATE")->load();
        lfo.setFrequency(rate);
    }
    else
    {
        // Calculate tempo-synced frequency
        float syncMultiplier = getSyncMultiplier(currentSyncMode);
        float syncedHz = (hostTempo / 60.0f) * syncMultiplier;
        lfo.setFrequency(syncedHz);
    }
}

float fieldEngineFXProcessor::getSyncMultiplier(SyncMode mode) const
{
    switch (mode)
    {
        case QUARTER:       return 1.0f;    // 1/4 note
        case EIGHTH:        return 2.0f;    // 1/8 note
        case SIXTEENTH:     return 4.0f;    // 1/16 note
        case THIRTYSECOND:  return 8.0f;    // 1/32 note
        default:            return 1.0f;
    }
}

void fieldEngineFXProcessor::updateFilterResponse()
{
    // Simple frequency response calculation for visualizer
    // This is a simplified version - in a real implementation you'd calculate
    // the actual filter response across frequency bins
    for (size_t i = 0; i < filterResponse.size(); ++i)
    {
        float freq = 20.0f * std::pow(1000.0f, static_cast<float>(i) / static_cast<float>(filterResponse.size() - 1));
        // Simulate filter response based on current morph position
        float response = 0.5f + 0.4f * std::sin(currentMorphPosition * 3.14159f + static_cast<float>(i) * 0.2f);
        filterResponse[i] = juce::jlimit(0.0f, 1.0f, response);
    }
}

juce::AudioProcessorEditor* fieldEngineFXProcessor::createEditor()
{
    return new fieldEngineFXEditor(*this);
}

void fieldEngineFXProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void fieldEngineFXProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
}

// Plugin instantiation
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new fieldEngineFXProcessor();
}