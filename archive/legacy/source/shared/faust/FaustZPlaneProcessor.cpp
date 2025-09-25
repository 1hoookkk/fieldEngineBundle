#include "FaustZPlaneProcessor.h"
// Include the Faust-generated header (will be created by faust2juce)
#include "zplane_morph.h"

FaustZPlaneProcessor::FaustZPlaneProcessor()
    : juce::AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true).withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, juce::Identifier("FaustZPlane"), createParameterLayout())
{
    // Initialize parameter smoothers
    morphSmoother.setCurrentAndTargetValue(0.0f);
    cutoffSmoother.setCurrentAndTargetValue(1000.0f);
    resonanceSmoother.setCurrentAndTargetValue(0.5f);
    driveSmoother.setCurrentAndTargetValue(0.0f);
    mixSmoother.setCurrentAndTargetValue(1.0f);
    lfoRateSmoother.setCurrentAndTargetValue(0.5f);
    lfoDepthSmoother.setCurrentAndTargetValue(0.0f);
}

FaustZPlaneProcessor::~FaustZPlaneProcessor() = default;

void FaustZPlaneProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/)
{
    // Initialize Faust processor
    faustProcessor = std::make_unique<mydsp>();
    faustProcessor->init(static_cast<int>(sampleRate));

    // Set up smoothers
    const double smoothingTimeMs = 20.0;
    const double smoothingRateHz = sampleRate / 1000.0; // Changed from samplesPerBlock to 1000.0

    morphSmoother.reset(sampleRate, smoothingTimeMs / 1000.0);
    cutoffSmoother.reset(sampleRate, smoothingTimeMs / 1000.0);
    resonanceSmoother.reset(sampleRate, smoothingTimeMs / 1000.0);
    driveSmoother.reset(sampleRate, smoothingTimeMs / 1000.0);
    mixSmoother.reset(sampleRate, smoothingTimeMs / 1000.0);
    lfoRateSmoother.reset(sampleRate, smoothingTimeMs / 1000.0);
    lfoDepthSmoother.reset(sampleRate, smoothingTimeMs / 1000.0);

    // Initialize parameter indices
    initializeParameterIndices();
}

void FaustZPlaneProcessor::releaseResources()
{
    // Clean up if needed
}

void FaustZPlaneProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    if (!faustProcessor)
        return;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // Update smoothed parameters
    updateFaustParameters();

    // Prepare input/output arrays for Faust
    float* inputs[2] = { nullptr, nullptr };
    float* outputs[2] = { nullptr, nullptr };

    for (int ch = 0; ch < std::min(numChannels, 2); ++ch)
    {
        inputs[ch] = buffer.getWritePointer(ch);
        outputs[ch] = buffer.getWritePointer(ch);
    }

    // Process with Faust
    faustProcessor->compute(numSamples, inputs, outputs);
}

bool FaustZPlaneProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::mono()
        || layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

void FaustZPlaneProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void FaustZPlaneProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr)
        if (xmlState->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
}

// Parameter setters - these are now just wrappers around ValueTreeState
void FaustZPlaneProcessor::setMorph(float value)
{
    parameters.getParameter("morph")->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, value));
}

void FaustZPlaneProcessor::setCutoff(float hz)
{
    parameters.getParameter("cutoff")->setValueNotifyingHost(juce::jlimit(20.0f, 20000.0f, hz));
}

void FaustZPlaneProcessor::setResonance(float q)
{
    parameters.getParameter("resonance")->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, q));
}

void FaustZPlaneProcessor::setDrive(float db)
{
    parameters.getParameter("drive")->setValueNotifyingHost(juce::jlimit(0.0f, 24.0f, db));
}

void FaustZPlaneProcessor::setMix(float mix)
{
    parameters.getParameter("mix")->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, mix));
}

void FaustZPlaneProcessor::setLFORate(float hz)
{
    parameters.getParameter("lfoRate")->setValueNotifyingHost(juce::jlimit(0.02f, 8.0f, hz));
}

void FaustZPlaneProcessor::setLFODepth(float depth)
{
    parameters.getParameter("lfoDepth")->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, depth));
}

void FaustZPlaneProcessor::updateFaustParameters()
{
    if (!faustProcessor)
        return;

    // Update Faust parameters using setParamValue()
    if (morphIndex >= 0)
        faustProcessor->setParamValue(morphIndex, *parameters.getRawParameterValue("morph"));
    if (cutoffIndex >= 0)
        faustProcessor->setParamValue(cutoffIndex, *parameters.getRawParameterValue("cutoff"));
    if (resonanceIndex >= 0)
        faustProcessor->setParamValue(resonanceIndex, *parameters.getRawParameterValue("resonance"));
    if (driveIndex >= 0)
        faustProcessor->setParamValue(driveIndex, *parameters.getRawParameterValue("drive"));
    if (mixIndex >= 0)
        faustProcessor->setParamValue(mixIndex, *parameters.getRawParameterValue("mix"));
    if (lfoRateIndex >= 0)
        faustProcessor->setParamValue(lfoRateIndex, *parameters.getRawParameterValue("lfoRate"));
    if (lfoDepthIndex >= 0)
        faustProcessor->setParamValue(lfoDepthIndex, *parameters.getRawParameterValue("lfoDepth"));
}

void FaustZPlaneProcessor::initializeParameterIndices()
{
    if (!faustProcessor)
        return;

    // Find parameter indices by name
    for (int i = 0; i < faustProcessor->getParamsCount(); ++i)
    {
        std::string paramName = faustProcessor->getParamLabel(i);
        if (paramName == "Morph") morphIndex = i;
        else if (paramName == "Intensity") resonanceIndex = i; // Faust uses "Intensity"
        else if (paramName == "Drive") driveIndex = i;
        else if (paramName == "Mix") mixIndex = i;
        else if (paramName == "LFO Rate") lfoRateIndex = i;
        else if (paramName == "LFO Depth") lfoDepthIndex = i;
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout FaustZPlaneProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>("morph", "Morph", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("cutoff", "Cutoff", 20.0f, 20000.0f, 1000.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("resonance", "Resonance", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("drive", "Drive", 0.0f, 24.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("mix", "Mix", 0.0f, 1.0f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("lfoRate", "LFO Rate", 0.02f, 8.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("lfoDepth", "LFO Depth", 0.0f, 1.0f, 0.0f));

    return { params.begin(), params.end() };
}