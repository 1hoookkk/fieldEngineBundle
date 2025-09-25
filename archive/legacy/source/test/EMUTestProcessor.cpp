#include "EMUTestProcessor.h"

EMUTestProcessor::EMUTestProcessor()
    : juce::AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , parameters(*this, nullptr, juce::Identifier("EMUTest"), createParameterLayout())
{
    // Get parameter pointers
    morphParam = dynamic_cast<juce::AudioParameterFloat*>(parameters.getParameter("morph"));
    intensityParam = dynamic_cast<juce::AudioParameterFloat*>(parameters.getParameter("intensity"));
    driveParam = dynamic_cast<juce::AudioParameterFloat*>(parameters.getParameter("drive"));
    saturationParam = dynamic_cast<juce::AudioParameterFloat*>(parameters.getParameter("saturation"));
    lfoRateParam = dynamic_cast<juce::AudioParameterFloat*>(parameters.getParameter("lfoRate"));
    lfoDepthParam = dynamic_cast<juce::AudioParameterFloat*>(parameters.getParameter("lfoDepth"));
    morphPairParam = dynamic_cast<juce::AudioParameterChoice*>(parameters.getParameter("morphPair"));
    autoMakeupParam = dynamic_cast<juce::AudioParameterBool*>(parameters.getParameter("autoMakeup"));
    bypassParam = dynamic_cast<juce::AudioParameterBool*>(parameters.getParameter("bypass"));
    toneOnParam = dynamic_cast<juce::AudioParameterBool*>(parameters.getParameter("toneOn"));
    toneFreqParam = dynamic_cast<juce::AudioParameterFloat*>(parameters.getParameter("toneFreq"));
    toneLevelDbParam = dynamic_cast<juce::AudioParameterFloat*>(parameters.getParameter("toneLevelDb"));
}

EMUTestProcessor::~EMUTestProcessor() = default;

const juce::String EMUTestProcessor::getName() const
{
    return "EMU Test";
}

bool EMUTestProcessor::acceptsMidi() const
{
    return false;
}

bool EMUTestProcessor::producesMidi() const
{
    return false;
}

bool EMUTestProcessor::isMidiEffect() const
{
    return false;
}

double EMUTestProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int EMUTestProcessor::getNumPrograms()
{
    return 1;
}

int EMUTestProcessor::getCurrentProgram()
{
    return 0;
}

void EMUTestProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String EMUTestProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void EMUTestProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

void EMUTestProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void EMUTestProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
}

void EMUTestProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Initialize EMU engine components
    shapeBank = std::make_unique<StaticShapeBank>();
    emuEngine = std::make_unique<AuthenticEMUEngine>(*shapeBank);
    oversampledEngine = std::make_unique<OversampledEngine>();

    // Prepare engines
    emuEngine->prepare(sampleRate, samplesPerBlock, 2);
    oversampledEngine->prepare(sampleRate, 2, OversampledEngine::Mode::OS2_IIR);
    oversampledEngine->setMaxBlock(samplesPerBlock);
    tonePhase = 0.0f;
    // If your host supports latency reporting via AudioProcessor, you could call setLatencySamples here
}

void EMUTestProcessor::releaseResources()
{
    emuEngine.reset();
    oversampledEngine.reset();
    shapeBank.reset();
}

bool EMUTestProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}

void EMUTestProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear unused output channels
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // Optional internal test tone
    if (toneOnParam != nullptr && toneOnParam->get())
    {
        const float freq = toneFreqParam != nullptr ? toneFreqParam->get() : 220.0f;
        const float levelDb = toneLevelDbParam != nullptr ? toneLevelDbParam->get() : -12.0f;
        const float levelLin = std::pow(10.0f, levelDb / 20.0f);
        const float fs = (float) getSampleRate();
        const float twoPi = juce::MathConstants<float>::twoPi;

        for (int n = 0; n < buffer.getNumSamples(); ++n)
        {
            const float sample = std::sin(tonePhase) * levelLin;
            tonePhase += twoPi * (freq / fs);
            if (tonePhase >= twoPi)
                tonePhase -= twoPi;

            for (int ch = 0; ch < juce::jmin(2, buffer.getNumChannels()); ++ch)
                buffer.getWritePointer(ch)[n] += sample;
        }
    }

    // Check bypass
    if (bypassParam->get())
        return;

    // Update EMU engine parameters
    ZPlaneParams params;
    params.morphPair = morphPairParam->getIndex();
    params.morph = morphParam->get();
    params.intensity = intensityParam->get();
    params.driveDb = driveParam->get();
    params.sat = saturationParam->get();
    params.lfoRate = lfoRateParam->get();
    params.lfoDepth = lfoDepthParam->get();
    params.autoMakeup = autoMakeupParam->get();
    
    emuEngine->setParams(params);

    // Process through EMU engine (oversampled nonlinear island)
    if (!emuEngine->isEffectivelyBypassed())
        oversampledEngine->process(*emuEngine, buffer, buffer.getNumSamples());
}

juce::AudioProcessorEditor* EMUTestProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this);
}

bool EMUTestProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorValueTreeState::ParameterLayout EMUTestProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    
    // Morph pair selection
    juce::StringArray morphPairChoices;
    morphPairChoices.add("Vowel Pair");
    morphPairChoices.add("Bell Pair");
    morphPairChoices.add("Low Pair");
    params.push_back(std::make_unique<juce::AudioParameterChoice>("morphPair", "Morph Pair", morphPairChoices, 0));
    
    // Morph control
    params.push_back(std::make_unique<juce::AudioParameterFloat>("morph", "Morph", 0.0f, 1.0f, 0.5f));
    
    // Intensity
    params.push_back(std::make_unique<juce::AudioParameterFloat>("intensity", "Intensity", 0.0f, 1.0f, 0.6f));
    
    // Drive
    params.push_back(std::make_unique<juce::AudioParameterFloat>("drive", "Drive", -20.0f, 20.0f, 0.0f));
    
    // Saturation
    params.push_back(std::make_unique<juce::AudioParameterFloat>("saturation", "Saturation", 0.0f, 1.0f, 0.0f));
    
    // LFO
    params.push_back(std::make_unique<juce::AudioParameterFloat>("lfoRate", "LFO Rate", 0.0f, 10.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("lfoDepth", "LFO Depth", 0.0f, 1.0f, 0.0f));
    
    // Auto makeup
    params.push_back(std::make_unique<juce::AudioParameterBool>("autoMakeup", "Auto Makeup", true));
    
    // Bypass
    params.push_back(std::make_unique<juce::AudioParameterBool>("bypass", "Bypass", false));

    // Built-in audition tone
    params.push_back(std::make_unique<juce::AudioParameterBool>("toneOn", "Tone On", true));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("toneFreq", "Tone Freq", 40.0f, 4000.0f, 220.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("toneLevelDb", "Tone Level", -60.0f, 0.0f, -12.0f));
    
    return { params.begin(), params.end() };
}

// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EMUTestProcessor();
}
