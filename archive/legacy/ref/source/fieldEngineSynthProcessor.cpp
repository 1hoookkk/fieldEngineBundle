#include "fieldEngineSynthProcessor.h"
#include "fieldEngineSynthEditor.h"

fieldEngineSynthProcessor::fieldEngineSynthProcessor()
    : juce::AudioProcessor(BusesProperties()
                          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "Parameters", createParameterLayout())
{
    // Initialize filter response array
    filterResponse.fill(0.0f);
    
    // Initialize voice management
    voiceActive.fill(false);
    voiceNote.fill(-1);
}

fieldEngineSynthProcessor::~fieldEngineSynthProcessor()
{
}

juce::AudioProcessorValueTreeState::ParameterLayout fieldEngineSynthProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Morph position (0.0 to 1.0)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "MORPH", "Morph", 0.0f, 1.0f, 0.5f));

    // Oscillator Detune (-12 to +12 semitones)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "DETUNE", "Detune", -12.0f, 12.0f, 0.0f));

    // Filter Cutoff (20Hz to 20kHz)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "CUTOFF", "Cutoff", 20.0f, 20000.0f, 1000.0f));

    // Filter Resonance (0.1 to 10.0)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "RESONANCE", "Resonance", 0.1f, 10.0f, 1.0f));

    // Attack (1ms to 5s)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "ATTACK", "Attack", 0.001f, 5.0f, 0.01f));

    // Decay (1ms to 5s)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "DECAY", "Decay", 0.001f, 5.0f, 0.3f));

    // Sustain (0.0 to 1.0)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "SUSTAIN", "Sustain", 0.0f, 1.0f, 0.7f));

    // Release (1ms to 10s)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "RELEASE", "Release", 0.001f, 10.0f, 1.0f));

    // Output Level
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "OUTPUT", "Output", 0.0f, 2.0f, 0.8f));

    return { params.begin(), params.end() };
}

void fieldEngineSynthProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    // Initialize voices
    for (auto& voice : voices)
    {
        voice.setSampleRate(static_cast<float>(sampleRate));
    }

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
}

void fieldEngineSynthProcessor::releaseResources()
{
    morphFilters.clear();
    emuFilters.clear();
}

bool fieldEngineSynthProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // Only support stereo output for synth
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void fieldEngineSynthProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear the buffer
    buffer.clear();

    // Get parameter values
    float morphPosition = parameters.getRawParameterValue("MORPH")->load();
    float detune = parameters.getRawParameterValue("DETUNE")->load();
    float cutoff = parameters.getRawParameterValue("CUTOFF")->load();
    float resonance = parameters.getRawParameterValue("RESONANCE")->load();
    float attack = parameters.getRawParameterValue("ATTACK")->load();
    float decay = parameters.getRawParameterValue("DECAY")->load();
    float sustain = parameters.getRawParameterValue("SUSTAIN")->load();
    float release = parameters.getRawParameterValue("RELEASE")->load();
    float output = parameters.getRawParameterValue("OUTPUT")->load();

    // Process MIDI messages
    for (const auto metadata : midiMessages)
    {
        auto message = metadata.getMessage();
        
        if (message.isNoteOn())
        {
            int note = message.getNoteNumber();
            float frequency = 440.0f * std::pow(2.0f, (note - 69 + detune) / 12.0f);
            
            // Find a free voice
            for (int v = 0; v < MAX_VOICES; ++v)
            {
                if (!voiceActive[v])
                {
                    voiceActive[v] = true;
                    voiceNote[v] = note;
                    voices[v].setFrequency(frequency);
                    voices[v].setAmplitude(0.7f); // Fixed amplitude for now
                    break;
                }
            }
        }
        else if (message.isNoteOff())
        {
            int note = message.getNoteNumber();
            
            // Find and stop the voice
            for (int v = 0; v < MAX_VOICES; ++v)
            {
                if (voiceActive[v] && voiceNote[v] == note)
                {
                    voiceActive[v] = false;
                    voiceNote[v] = -1;
                    voices[v].setAmplitude(0.0f);
                    break;
                }
            }
        }
    }

    // Update filter parameters
    for (auto& morphFilter : morphFilters)
    {
        morphFilter->setMorph(morphPosition);
    }

    // Generate audio from active voices
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        float monoSample = 0.0f;
        
        // Sum all active voices
        for (int v = 0; v < MAX_VOICES; ++v)
        {
            if (voiceActive[v])
            {
                monoSample += voices[v].generateSample(static_cast<float>(currentSampleRate));
            }
        }
        
        // Apply to both channels
        for (int channel = 0; channel < totalNumOutputChannels && channel < 2; ++channel)
        {
            auto* channelData = buffer.getWritePointer(channel);
            
            // Process through EMU filter
            float processedSample = emuFilters[channel]->processSample(monoSample);
            
            // Apply output level
            channelData[sample] = processedSample * output;
            
            // Update visualizer data (first channel only)
            if (channel == 0)
            {
                currentMorphPosition = morphPosition;
                currentLFOValue = 0.0f; // No LFO in basic synth version
                
                // Simple envelope follower for visualizer
                float rectified = std::abs(channelData[sample]);
                currentEnvelopeValue = currentEnvelopeValue * 0.999f + rectified * 0.001f;
            }
        }
    }

    // Process through morph filters using block processing
    for (size_t i = 0; i < morphFilters.size() && i < 2; ++i)
    {
        morphFilters[i]->process(buffer);
    }
}

juce::AudioProcessorEditor* fieldEngineSynthProcessor::createEditor()
{
    return new fieldEngineSynthEditor(*this);
}

void fieldEngineSynthProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void fieldEngineSynthProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
}

// Plugin instantiation
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new fieldEngineSynthProcessor();
}