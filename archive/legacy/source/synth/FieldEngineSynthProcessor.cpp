#include "FieldEngineSynthProcessor.h"
#include "FieldEngineSynthEditor.h"

namespace {
static inline float dbToGain(float db) { return juce::Decibels::decibelsToGain(db); }
}

FieldEngineSynthProcessor::FieldEngineSynthProcessor()
: juce::AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
, parameters (*this, nullptr, juce::Identifier("FieldEngineSynth"), createParameterLayout())
{
}

FieldEngineSynthProcessor::~FieldEngineSynthProcessor() = default;

void FieldEngineSynthProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);
    for (auto& v : voices) {
        v.setSampleRate((float) sampleRate);
        v.setTargetAmplitude(0.0f);
    }
    for (auto& e : envelopes) {
        e.setSampleRate(sampleRate);
    }
    for (auto& f : channelFilters) {
        f.prepareToPlay(sampleRate);
        f.setMorphPair(AuthenticEMUZPlane::VowelAe_to_VowelOo);
        f.setMorphPosition(0.5f);
        f.setIntensity(0.7f);
    }
    
    cutoffSmoother.reset(sampleRate, 0.05);
    resonanceSmoother.reset(sampleRate, 0.05);
    
    envelopeFollower = 0.0f;
}

void FieldEngineSynthProcessor::releaseResources() {}

bool FieldEngineSynthProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}

void FieldEngineSynthProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals _;
    buffer.clear();

    // Read parameters safely
    auto getParam = [this](const juce::String& id, float defVal){ if (auto* p = parameters.getRawParameterValue(id)) return p->load(); return defVal; };

    float detune = getParam("DETUNE", 0.0f);
    cutoffSmoother.setTargetValue(getParam("CUTOFF", 1000.0f));
    resonanceSmoother.setTargetValue(getParam("RESONANCE", 1.0f));
    float attack = getParam("ATTACK", 0.01f);
    float decay = getParam("DECAY", 0.3f);
    float sustain = getParam("SUSTAIN", 0.7f);
    float release = getParam("RELEASE", 1.0f);
    float outGain = getParam("OUTPUT", 0.8f);

    for (auto& e : envelopes)
        e.setParameters(attack, decay, sustain, release);

    // Simple MIDI handling with voice stealing
    for (const auto metadata : midi)
    {
        const auto m = metadata.getMessage();
        if (m.isNoteOn())
        {
            int note = m.getNoteNumber();
            
            // Find a free voice or steal the oldest
            int voiceToUse = -1;
            for (int i = 0; i < MAX_VOICES; ++i) {
                if (!envelopes[i].isActive()) {
                    voiceToUse = i;
                    break;
                }
            }
            if (voiceToUse == -1) {
                voiceToUse = 0;
                double oldestTime = voiceStartTime[0];
                for (int i = 1; i < MAX_VOICES; ++i) {
                    if (voiceStartTime[i] < oldestTime) {
                        oldestTime = voiceStartTime[i];
                        voiceToUse = i;
                    }
                }
            }
            
            voiceNote[voiceToUse] = note;
            voiceStartTime[voiceToUse] = juce::Time::getMillisecondCounterHiRes();
            float freq = juce::MidiMessage::getMidiNoteInHertz(note + (int)detune);
            voices[voiceToUse].setFrequency(freq);
            envelopes[voiceToUse].noteOn();
        }
        else if (m.isNoteOff())
        {
            int note = m.getNoteNumber();
            for (int i = 0; i < MAX_VOICES; ++i) {
                if (voiceNote[i] == note) {
                    envelopes[i].noteOff();
                }
            }
        }
    }

    const int numSamples = buffer.getNumSamples();
    std::vector<float> mono; mono.resize((size_t)numSamples, 0.0f);
    for (int i = 0; i < MAX_VOICES; ++i)
    {
        if (envelopes[i].isActive()) {
            for (int s = 0; s < numSamples; ++s)
                mono[(size_t)s] += voices[i].generateSample() * envelopes[i].getNextSample();
        }
    }

    for (auto& f : channelFilters)
    {
        f.setMorphPosition(cutoffSmoother.getNextValue() / 22000.0f);  // Normalize to 0-1
        f.setIntensity(juce::jlimit(0.0f, 1.0f, resonanceSmoother.getNextValue() / 10.0f));
    }

    int totalChannels = getTotalNumOutputChannels();
    for (int ch = 0; ch < totalChannels; ++ch)
    {
        auto* out = buffer.getWritePointer(ch);
        for (int s = 0; s < numSamples; ++s)
        {
            float x = mono[(size_t)s];
            float y = channelFilters[(size_t)juce::jmin(ch, 1)].processSample(x);
            out[s] = y;
        }
    }

    buffer.applyGain(juce::jlimit(0.0f, 2.0f, outGain));

    float env = envelopeFollower;
    const float a = 0.01f;
    auto* left = buffer.getReadPointer(0);
    for (int s = 0; s < numSamples; ++s) env += a * (std::abs(left[s]) - env);
    envelopeFollower = juce::jlimit(0.0f, 1.0f, env);
}

juce::AudioProcessorEditor* FieldEngineSynthProcessor::createEditor()
{
    return new FieldEngineSynthEditor(*this);
}

void FieldEngineSynthProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void FieldEngineSynthProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorValueTreeState::ParameterLayout FieldEngineSynthProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    auto add = [&params](auto p){ params.push_back(std::move(p)); };

    add (std::make_unique<juce::AudioParameterFloat>("DETUNE", "DETUNE", juce::NormalisableRange<float>(-12.0f, 12.0f), 0.0f));
    add (std::make_unique<juce::AudioParameterFloat>("CUTOFF", "CUTOFF", juce::NormalisableRange<float>(20.0f, 20000.0f, 0.0f, 0.3f), 1000.0f));
    add (std::make_unique<juce::AudioParameterFloat>("RESONANCE", "RESONANCE", juce::NormalisableRange<float>(0.1f, 10.0f, 0.0f, 0.5f), 1.0f));
    add (std::make_unique<juce::AudioParameterFloat>("ATTACK", "ATTACK", juce::NormalisableRange<float>(0.001f, 5.0f, 0.0f, 0.5f), 0.01f));
    add (std::make_unique<juce::AudioParameterFloat>("DECAY", "DECAY", juce::NormalisableRange<float>(0.001f, 5.0f, 0.0f, 0.5f), 0.3f));
    add (std::make_unique<juce::AudioParameterFloat>("SUSTAIN", "SUSTAIN", juce::NormalisableRange<float>(0.0f, 1.0f), 0.7f));
    add (std::make_unique<juce::AudioParameterFloat>("RELEASE", "RELEASE", juce::NormalisableRange<float>(0.001f, 10.0f, 0.0f, 0.5f), 1.0f));
    add (std::make_unique<juce::AudioParameterFloat>("OUTPUT", "OUTPUT", juce::NormalisableRange<float>(0.0f, 2.0f), 0.8f));

    return { params.begin(), params.end() };
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FieldEngineSynthProcessor();
}
