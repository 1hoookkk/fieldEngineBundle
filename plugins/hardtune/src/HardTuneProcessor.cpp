#include "HardTuneProcessor.h"
#include "HardTuneUI.h"

#include <cmath>
#include <algorithm>

namespace
{
    constexpr int kMinColourChannels = 2;
}

HardTuneProcessor::HardTuneProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createParameterLayout())
{
    modeParam      = apvts.getRawParameterValue("mode");
    retuneParam    = apvts.getRawParameterValue("retune");
    amountParam    = apvts.getRawParameterValue("amount");
    keyParam       = apvts.getRawParameterValue("key");
    scaleParam     = apvts.getRawParameterValue("scale");
    colorParam     = apvts.getRawParameterValue("color");
    formantParam   = apvts.getRawParameterValue("formant");
    throatParam    = apvts.getRawParameterValue("throat");
    mixParam       = apvts.getRawParameterValue("mix");
    biasParam      = apvts.getRawParameterValue("bias");
    inputTypeParam = apvts.getRawParameterValue("inputType");
}

juce::AudioProcessorValueTreeState::ParameterLayout HardTuneProcessor::createParameterLayout()
{
    using Choice = juce::AudioParameterChoice;
    using Float  = juce::AudioParameterFloat;
    using Range  = juce::NormalisableRange<float>;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<Choice>("mode", "Mode", juce::StringArray{"Track", "Print"}, 0));
    params.push_back(std::make_unique<Float>("retune", "Retune Speed", Range(0.0f, 1.0f, 0.0001f), 1.0f));
    params.push_back(std::make_unique<Float>("amount", "Correction Amount", Range(0.0f, 1.0f, 0.0001f), 1.0f));

    params.push_back(std::make_unique<Choice>(
        "key", "Key",
        juce::StringArray{"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"},
        0));

    params.push_back(std::make_unique<Choice>(
        "scale", "Scale", juce::StringArray{"Chromatic", "Major", "Minor"}, 1));

    params.push_back(std::make_unique<Float>("color", "Color", Range(0.0f, 1.0f, 0.0001f), 0.15f));
    params.push_back(std::make_unique<Float>("formant", "Formant Shift (st)", Range(-12.0f, 12.0f, 0.01f), 0.0f));
    params.push_back(std::make_unique<Float>("throat", "Throat Ratio", Range(0.5f, 2.0f, 0.001f), 1.0f));
    params.push_back(std::make_unique<Float>("mix", "Mix", Range(0.0f, 1.0f, 0.0001f), 1.0f));
    params.push_back(std::make_unique<Choice>("bias", "Note Bias", juce::StringArray{"Nearest", "Up", "Down"}, 0));
    params.push_back(std::make_unique<Choice>(
        "inputType", "Input Type",
        juce::StringArray{"Soprano", "Alto", "Tenor", "Baritone", "Bass"},
        2));

    return { params.begin(), params.end() };
}

void HardTuneProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    preparedBlockSize = juce::jmax(1, samplesPerBlock);

    ensureCapacity(preparedBlockSize, getTotalNumOutputChannels());

    pitchEngine.prepare(sampleRate, preparedBlockSize);
    formantShifter.prepare(sampleRate, preparedBlockSize);
    shifter.prepare(sampleRate, Shifter::TrackPSOLA);

    colorStage.prepareToPlay(sampleRate);
    colorStage.setMorphPair(0);
    colorStage.setMorphPosition(0.5f);
    colorStage.setAutoMakeup(true);

    lastKey = lastScale = lastInputType = lastMode = -1;
    lastBias = std::numeric_limits<int>::min();
    lastRetune = std::numeric_limits<float>::quiet_NaN();
    lastColor = std::numeric_limits<float>::quiet_NaN();

    refreshKeyScale(true);
    refreshRetune(true);
    refreshInputRange(true);
    refreshMode(true);
    updateColor(colorParam->load(), true);
}

void HardTuneProcessor::releaseResources()
{
    monoBuffer.clear();
    ratioBuffer.clear();
    wetBuffer.setSize(0, 0);
}

bool HardTuneProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    auto mainIn  = layouts.getMainInputChannelSet();
    auto mainOut = layouts.getMainOutputChannelSet();

    if (mainIn != mainOut)
        return false;

    return mainIn == juce::AudioChannelSet::mono()
        || mainIn == juce::AudioChannelSet::stereo();
}

juce::String HardTuneProcessor::getCurrentPresetName() const
{
    if (currentPresetIndex >= 0 && currentPresetIndex < (int) factoryPresets.size())
        return factoryPresets[(size_t) currentPresetIndex].name;
    return "Custom";
}

int HardTuneProcessor::getNumPresets() const
{
    return (int) factoryPresets.size();
}

juce::String HardTuneProcessor::getPresetName(int index) const
{
    if (index >= 0 && index < (int) factoryPresets.size())
        return factoryPresets[(size_t) index].name;
    return {};
}

void HardTuneProcessor::loadPreset(int index)
{
    if (index < 0 || index >= (int) factoryPresets.size())
        return;

    const auto& preset = factoryPresets[(size_t) index];
    auto apply = [this](const juce::String& id, float value)
    {
        if (auto* param = apvts.getParameter(id))
        {
            param->beginChangeGesture();
            const auto norm = param->getNormalisableRange().convertTo0to1(value);
            param->setValueNotifyingHost(norm);
            param->endChangeGesture();
        }
    };

    apply("mode",   (float) preset.mode);
    apply("retune", preset.retune);
    apply("amount", preset.amount);
    apply("mix",    preset.mix);
    apply("color",  preset.color);
    apply("formant", preset.formant);
    apply("throat",  preset.throat);
    apply("key",     (float) preset.key);
    apply("scale",   (float) preset.scale);
    apply("bias",    (float) preset.bias);
    apply("inputType", (float) preset.inputType);

    currentPresetIndex = index;
}

void HardTuneProcessor::initializeFactoryPresets()
{
    factoryPresets = {
        { "LIVE:Natural", 0, 0.65f, 0.70f, 0.75f, 0.18f, 0.0f, 1.00f, 0, 1, 0, 2 },
        { "LIVE:Tight",   0, 0.85f, 0.90f, 0.90f, 0.20f, 0.0f, 1.00f, 0, 1, 0, 2 },
        { "LIVE:Hard",    0, 1.00f, 1.00f, 1.00f, 0.25f, 0.0f, 1.00f, 0, 0, 0, 2 },
        { "STUDIO:Gentle",0, 0.40f, 0.55f, 0.60f, 0.12f, 0.0f, 1.00f, 0, 1, 0, 2 },
        { "STUDIO:Double",1, 0.80f, 0.85f, 0.65f, 0.22f, 0.0f, 1.05f, 0, 1, 0, 2 },
        { "CREATIVE:Robot",0, 1.00f, 1.00f, 1.00f, 0.30f, 0.0f, 0.80f, 0, 0, 2, 2 },
        { "CREATIVE:Wide", 0, 0.75f, 0.80f, 0.90f, 0.28f, 2.0f, 1.15f, 0, 1, 0, 3 }
    };
}

void HardTuneProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numSamples == 0)
        return;

    if (numSamples > preparedBlockSize)
    {
        preparedBlockSize = numSamples;
        pitchEngine.prepare(currentSampleRate, preparedBlockSize);
        formantShifter.prepare(currentSampleRate, preparedBlockSize);
        ensureCapacity(preparedBlockSize, juce::jmax(numChannels, kMinColourChannels));
        refreshKeyScale(true);
        refreshRetune(true);
        refreshInputRange(true);
        refreshMode(true);
    }

    ensureCapacity(numSamples, juce::jmax(numChannels, kMinColourChannels));

    refreshKeyScale();
    refreshRetune();
    refreshInputRange();
    refreshMode();

    const float colour = colorParam->load();
    if (! juce::approximatelyEqual(colour, lastColor, 1.0e-4f))
        updateColor(colour);

    const float amount    = juce::jlimit(0.0f, 1.0f, amountParam->load());
    const float mix       = juce::jlimit(0.0f, 1.0f, mixParam->load());
    const float formantSt = formantParam->load();
    const float throat    = juce::jlimit(0.5f, 2.0f, throatParam->load());

    // Mono detection buffer
    if (numChannels == 1)
    {
        const float* src = buffer.getReadPointer(0);
        std::copy(src, src + numSamples, monoBuffer.begin());
    }
    else
    {
        const float* left  = buffer.getReadPointer(0);
        const float* right = buffer.getReadPointer(1);
        for (int i = 0; i < numSamples; ++i)
            monoBuffer[(size_t)i] = 0.5f * (left[i] + right[i]);
    }

    const auto block = pitchEngine.analyze(monoBuffer.data(), numSamples);

    if (block.ratio != nullptr)
        std::copy(block.ratio, block.ratio + numSamples, ratioBuffer.begin());
    else
        std::fill(ratioBuffer.begin(), ratioBuffer.begin() + numSamples, 1.0f);

    const float guard = sibilantGuard.weight(monoBuffer.data(), numSamples);
    const float appliedAmount = juce::jlimit(0.0f, 1.0f, amount * guard);

    for (int n = 0; n < numSamples; ++n)
    {
        const float ratio = ratioBuffer[(size_t)n];
        const float blended = 1.0f + (ratio - 1.0f) * appliedAmount;
        ratioBuffer[(size_t)n] = juce::jlimit(0.25f, 4.0f, blended);
    }

    wetBuffer.clear();

    const float formantRatio = throat * std::pow(2.0f, formantSt / 12.0f);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        const float* in  = buffer.getReadPointer(ch);
        float*        wet = wetBuffer.getWritePointer(ch);

        shifter.processBlock(in, wet, numSamples, ratioBuffer.data(), block.f0);

        if (std::abs(formantRatio - 1.0f) > 0.01f)
            formantShifter.process(wet, numSamples, formantRatio);
    }

    colorStage.process(wetBuffer);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* out = buffer.getWritePointer(ch);
        const float* wet = wetBuffer.getReadPointer(ch);

        for (int n = 0; n < numSamples; ++n)
        {
            const float drySample = out[n];
            out[n] = mix * wet[n] + (1.0f - mix) * drySample;
        }
    }
}

juce::AudioProcessorEditor* HardTuneProcessor::createEditor()
{
    return new HardTuneUI(*this);
}

void HardTuneProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream mos(destData, true);
    apvts.state.writeToStream(mos);
}

void HardTuneProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto tree = juce::ValueTree::readFromData(data, (size_t) sizeInBytes))
    {
        apvts.replaceState(tree);
        refreshKeyScale(true);
        refreshRetune(true);
        refreshInputRange(true);
        refreshMode(true);
        updateColor(colorParam->load(), true);
    }
}

void HardTuneProcessor::ensureCapacity(int numSamples, int numChannels)
{
    if ((int) monoBuffer.size() < numSamples)
        monoBuffer.resize((size_t) numSamples, 0.0f);

    if ((int) ratioBuffer.size() < numSamples)
        ratioBuffer.resize((size_t) numSamples, 1.0f);

    const int requiredChannels = juce::jmax(kMinColourChannels, numChannels);
    if (wetBuffer.getNumChannels() != requiredChannels
        || wetBuffer.getNumSamples() < numSamples)
    {
        wetBuffer.setSize(requiredChannels, numSamples, false, false, false);
    }
}

void HardTuneProcessor::refreshKeyScale(bool force)
{
    const int key   = (int) keyParam->load();
    const int scale = (int) scaleParam->load();

    if (! force && key == lastKey && scale == lastScale)
        return;

    pitchEngine.setKeyScale(key, maskForScale(scale));
    lastKey = key;
    lastScale = scale;
}

void HardTuneProcessor::refreshRetune(bool force)
{
    const float retuneRaw = juce::jlimit(0.0f, 1.0f, retuneParam->load());
    const int   biasRaw   = biasForChoice((int) biasParam->load());

    if (! force && juce::approximatelyEqual(retuneRaw, lastRetune, 1.0e-5f) && biasRaw == lastBias)
        return;

    pitchEngine.setRetune(retuneRaw, biasRaw);
    lastRetune = retuneRaw;
    lastBias = biasRaw;
}

void HardTuneProcessor::refreshInputRange(bool force)
{
    const int type = (int) inputTypeParam->load();

    if (! force && type == lastInputType)
        return;

    const auto [fMin, fMax] = rangeForInputType(type);
    pitchEngine.setRange(fMin, fMax);
    lastInputType = type;
}

void HardTuneProcessor::refreshMode(bool force)
{
    const int modeChoice = (int) modeParam->load();

    if (! force && modeChoice == lastMode)
        return;

    const auto mode = (modeChoice == 0) ? Shifter::TrackPSOLA : Shifter::PrintHQ;
    shifter.prepare(currentSampleRate, mode);
    lastMode = modeChoice;
}

void HardTuneProcessor::updateColor(float colourAmount, bool force)
{
    if (! force && juce::approximatelyEqual(colourAmount, lastColor, 1.0e-5f))
        return;

    colorStage.setIntensity(colourAmount);
    colorStage.setDrive(juce::jmap(colourAmount, 0.0f, 1.0f, 0.0f, 3.0f));
    colorStage.setSectionSaturation(juce::jmin(0.35f, 0.25f * colourAmount));
    colorStage.setLFODepth(0.0f);
    colorStage.setMorphPosition(0.5f);

    lastColor = colourAmount;
}

uint16_t HardTuneProcessor::maskForScale(int scaleIndex)
{
    switch (scaleIndex)
    {
        case 1: return 0x0AB5; // Major
        case 2: return 0x05AD; // Minor
        default: return 0x0FFF; // Chromatic
    }
}

int HardTuneProcessor::biasForChoice(int choice)
{
    switch (choice)
    {
        case 1: return 1;   // Up
        case 2: return -1;  // Down
        default: return 0;  // Nearest
    }
}

std::pair<float, float> HardTuneProcessor::rangeForInputType(int typeIndex)
{
    switch (typeIndex)
    {
        case 0: return { 165.0f, 1100.0f }; // Soprano
        case 1: return { 130.0f, 880.0f  }; // Alto
        case 2: return { 98.0f,  660.0f  }; // Tenor
        case 3: return { 82.0f,  520.0f  }; // Baritone
        case 4: return { 65.0f,  392.0f  }; // Bass
        default: return { 80.0f, 800.0f };  // Fallback
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HardTuneProcessor();
}
