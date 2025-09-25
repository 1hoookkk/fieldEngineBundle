#include "MorphEngineAudioProcessor.h"
#include "PremiumMorphUI.h"

namespace
{
    inline float getParamValue (const juce::AudioProcessorValueTreeState& state, const juce::String& paramID)
    {
        if (auto* p = state.getRawParameterValue (paramID))
            return p->load();

        jassertfalse; // missing parameter wiring
        return 0.0f;
    }

    inline float eqPowerWet (float mix) noexcept
    {
        mix = juce::jlimit (0.0f, 1.0f, mix);
        return std::sin (mix * juce::MathConstants<float>::halfPi);
    }

    inline float eqPowerDry (float mix) noexcept
    {
        mix = juce::jlimit (0.0f, 1.0f, mix);
        return std::cos (mix * juce::MathConstants<float>::halfPi);
    }

    template <typename SmoothedValue>
    float smoothValue (SmoothedValue& smoother, float target, int samples)
    {
        smoother.setTargetValue (target);
        float value = smoother.getNextValue();
        if (samples > 1)
            smoother.skip (samples - 1);
        return value;
    }
}

//==============================================================================
MorphEngineAudioProcessor::MorphEngineAudioProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    initializeFactoryPresets();

    styleListener = std::make_unique<StyleParamListener> (*this);
    apvts.addParameterListener ("style.variant", styleListener.get());
}

MorphEngineAudioProcessor::~MorphEngineAudioProcessor()
{
    if (styleListener != nullptr)
        apvts.removeParameterListener ("style.variant", styleListener.get());
    cancelPendingUpdate();
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout MorphEngineAudioProcessor::createParameterLayout()
{
    using Choice = juce::AudioParameterChoice;
    using Float  = juce::AudioParameterFloat;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<Choice> (
        "style.variant", "Style", juce::StringArray { "Air", "Liquid", "Punch" }, 0));

    params.push_back (std::make_unique<Float> (
        "zplane.morph", "Morph",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f, 0.8f), 0.28f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(juce::roundToInt(value * 100.0f)) + "%"; }));

    params.push_back (std::make_unique<Float> (
        "zplane.resonance", "Resonance",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f, 0.7f), 0.18f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(juce::roundToInt(value * 100.0f)) + "%"; }));

    params.push_back (std::make_unique<Float> (
        "tilt.brightness", "Brightness",
        juce::NormalisableRange<float> (-6.0f, 6.0f, 0.1f, 1.0f), 0.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(value, 1) + " dB"; }));

    params.push_back (std::make_unique<Float> (
        "drive.db", "Drive",
        juce::NormalisableRange<float> (0.0f, 12.0f, 0.1f, 0.5f), 0.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(value, 1) + " dB"; }));

    params.push_back (std::make_unique<Float> (
        "hardness", "Hardness",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f, 1.0f), 0.2f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(juce::roundToInt(value * 100.0f)) + "%"; }));

    params.push_back (std::make_unique<Float> (
        "style.mix", "Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f, 1.0f), 0.35f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(juce::roundToInt(value * 100.0f)) + "%"; }));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "safe.mode", "Safe Mode", true));

    params.push_back (std::make_unique<Float> (
        "output.trim", "Output Trim",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f, 0.5f), 0.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(value, 1) + " dB"; }));

    params.push_back (std::make_unique<Choice> (
        "quality.mode", "Quality", juce::StringArray { "Track", "Print" }, 1));

    

    params.push_back (std::make_unique<juce::AudioParameterChoice>(
        "motion.source", "Motion Source", juce::StringArray { "Off", "LFO Sync", "LFO Hz" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterChoice>(
        "motion.division", "Division", juce::StringArray { "1", "1/2", "1/2.", "1/2T", "1/4", "1/4.", "1/4T", "1/8", "1/8.", "1/8T", "1/16", "1/16.", "1/16T", "1/32" }, 4));
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        "motion.depth", "Motion Depth", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterBool>(
        "motion.retrig", "Retrigger", false));
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        "motion.hz", "Motion Rate (Hz)", juce::NormalisableRange<float>(0.05f, 8.0f, 0.0f, 0.3f), 0.5f));

    return { params.begin(), params.end() };
}

int MorphEngineAudioProcessor::styleToMorphPair (int styleIndex) noexcept
{
    switch (styleIndex)
    {
        case 0: return 0;
        case 1: return 1;
        case 2: default: return 2;
    }
}

void MorphEngineAudioProcessor::updateStyleState (int styleIndex)
{
    const int clampedStyle = juce::jlimit (0, 2, styleIndex);
    const int targetPair = styleToMorphPair (clampedStyle);

    if (targetPair != lastMorphPair.load())
    {
        emu.setMorphPair (static_cast<AuthenticEMUZPlane::MorphPair>(targetPair));
        lastMorphPair.store (targetPair);
    }

    lastStyleVariant.store (clampedStyle);
}

void MorphEngineAudioProcessor::initialiseSmoothers()
{
    auto init = [this](juce::LinearSmoothedValue<float>& smoother, const juce::String& id, double timeSeconds)
    {
        smoother.reset (currentSampleRate, timeSeconds);
        smoother.setCurrentAndTargetValue (getParamValue (apvts, id));
    };

    init (smMorph,     "zplane.morph",     0.02);
    init (smResonance, "zplane.resonance", 0.04);
    init (smDrive,     "drive.db",         0.03);
    init (smHardness,  "hardness",         0.03);
    init (smMix,       "style.mix",        0.01);
    init (smTilt,      "tilt.brightness",  0.05);

    smTrim.reset (currentSampleRate, 0.02);
    smTrim.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (getParamValue (apvts, "output.trim")));
}

//==============================================================================
void MorphEngineAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Attempt to load precise Audity model pack at runtime (falls back to compiled tables)
#if defined(LOAD_AUDITY_MODEL_PACK)
    (void) loadAudityModelPack(*this, emu);
#endif

    currentSampleRate = sampleRate;
    currentBlockSize  = samplesPerBlock;

    const int numChannels = juce::jmax (1, getTotalNumOutputChannels());
    dryScratch.setSize (numChannels, samplesPerBlock, false, false, true);

    {
        const juce::SpinLock::ScopedLockType lock (analysisLock);
        analysisBuffer.fill (0.0f);
        analysisWritePos = 0;
        analysisValidSamples = 0;
    }

    if (oversampler == nullptr)
        oversampler = std::make_unique<juce::dsp::Oversampling<float>> (numChannels, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);

    oversampleChannelPointers.resize ((size_t) numChannels);

    safeHighCut.reset();
    outputPeak.store (0.0f);
    clipHold.store (0);
    lastSafeModeState = (getParamValue (apvts, "safe.mode") > 0.5f);

    configureQuality (static_cast<Quality> ((int) getParamValue (apvts, "quality.mode")), samplesPerBlock);
    initialiseSmoothers();

    lastStyleVariant.store (-1);
    lastMorphPair.store (-1);
    updateStyleState ((int) getParamValue (apvts, "style.variant"));

    pendingStyleMacro.store (-1);
    publishFilterFrame();
}

void MorphEngineAudioProcessor::releaseResources()
{
    dryScratch.setSize (0, 0);
    if (oversampler != nullptr)
        oversampler->reset();
}

bool MorphEngineAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == layouts.getMainOutputChannelSet();
}

void MorphEngineAudioProcessor::configureQuality (Quality newQuality, int newBlockSize)
{
    quality = newQuality;
    oversampleFactor = (quality == Quality::Print ? 2 : 1);

    if (oversampler != nullptr)
    {
        oversampler->reset();
        oversampler->initProcessing ((size_t) newBlockSize);
    }

    printLowpass.reset();
    if (oversampleFactor > 1)
    {
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = currentSampleRate * oversampleFactor;
        spec.maximumBlockSize = (juce::uint32) (newBlockSize * oversampleFactor);
        spec.numChannels = (juce::uint32) juce::jmax (1, getTotalNumOutputChannels());

        printLowpass.prepare (spec);
        auto coeff = juce::dsp::IIR::Coefficients<float>::makeLowPass (spec.sampleRate, juce::jmin (18000.0, 0.45 * spec.sampleRate));
        *printLowpass.state = *coeff;
    }

    const int latencySamples = (quality == Quality::Print && oversampler != nullptr)
                                   ? (int) oversampler->getLatencyInSamples()
                                   : 0;

    setLatencySamples (latencySamples);

    emu.prepareToPlay (currentSampleRate * oversampleFactor);
    tilt.prepare (currentSampleRate * oversampleFactor);
    tilt.reset();
    tilt.setAmount (getParamValue (apvts, "tilt.brightness"));

    juce::dsp::ProcessSpec safeSpec;
    safeSpec.sampleRate = currentSampleRate;
    safeSpec.maximumBlockSize = (juce::uint32) juce::jmax (newBlockSize, 1);
    safeSpec.numChannels = (juce::uint32) juce::jmax (1, getTotalNumOutputChannels());
    safeHighCut.prepare (safeSpec);
    safeHighCut.reset();
    auto safeCoeff = juce::dsp::IIR::Coefficients<float>::makeLowPass (safeSpec.sampleRate, juce::jmin (16000.0, 0.45 * safeSpec.sampleRate));
    *safeHighCut.state = *safeCoeff;

}

//==============================================================================
void MorphEngineAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    // Motion (LFO) control mapped to EMU engine
    const int motionSource = (int) getParamValue (apvts, "motion.source");
    const float motionDepth = getParamValue (apvts, "motion.depth");
    const bool motionActive = (motionDepth > 1.0e-4f) && (motionSource == 1 || motionSource == 2);
    float lfoHz = 0.0f;
    if (motionActive) {
        if (motionSource == 1) {
            auto* ph = getPlayHead();
            double bpm = 120.0; bool isPlaying = lastWasPlaying;
            if (ph != nullptr)
            {
                if (auto pos = ph->getPosition())
                {
                    if (auto bpmValue = pos->getBpm(); bpmValue.hasValue())
                        bpm = juce::jmax (1.0, bpmValue.orFallback (120.0));
                    isPlaying = pos->getIsPlaying();
                }
            }
            const int divIndex = (int) getParamValue (apvts, "motion.division");
            static const double baseBeats[] = { 4.0, 2.0, 2.0, 2.0, 1.0, 1.0, 1.0, 0.5, 0.5, 0.5, 0.25, 0.25, 0.25, 0.125 };
            static const double dotted[]    = { 1.0, 1.0, 1.5, 2.0/3.0, 1.0, 1.5, 2.0/3.0, 1.0, 1.5, 2.0/3.0, 1.0, 1.5, 2.0/3.0, 1.0 };
            const int clampedIndex = juce::jlimit (0, 13, divIndex);
            const double beats = baseBeats[clampedIndex] * dotted[clampedIndex];
            const double periodSec = (60.0 / juce::jmax (1.0, bpm)) * beats;
            lfoHz = (float) juce::jlimit (0.02, 8.0, 1.0 / std::max (1e-3, periodSec));
            const bool retrig = (getParamValue (apvts, "motion.retrig") > 0.5f);
            if (retrig && ! lastWasPlaying && isPlaying) emu.setLFOPhase (0.0f);
            lastWasPlaying = isPlaying;
        } else {
            lfoHz = getParamValue (apvts, "motion.hz");
        }
        emu.setLFORate (lfoHz);
        emu.setLFODepth (juce::jlimit (0.0f, 1.0f, motionDepth));
    } else {
        emu.setLFORate (0.0f);
        emu.setLFODepth (0.0f);
    }


    const int styleIndex = (int) getParamValue (apvts, "style.variant");
    if (styleIndex != lastStyleVariant.load())
        updateStyleState (styleIndex);

    if (dryScratch.getNumChannels() != numChannels || dryScratch.getNumSamples() < numSamples)
        dryScratch.setSize (numChannels, numSamples, false, false, true);

    for (int ch = 0; ch < numChannels; ++ch)
        dryScratch.copyFrom (ch, 0, buffer, ch, 0, numSamples);

    const Quality desiredQuality = static_cast<Quality> ((int) getParamValue (apvts, "quality.mode"));
    if (desiredQuality != quality || numSamples > currentBlockSize)
    {
        // Queue quality change for async processing to avoid RT allocation
        const juce::SpinLock::ScopedLockType lock (qualityUpdateLock);
        pendingQualityUpdate.quality = desiredQuality;
        pendingQualityUpdate.blockSize = juce::jmax (currentBlockSize, numSamples);
        pendingQualityUpdate.pending = true;
        triggerAsyncUpdate();

        // For immediate processing, use current settings until async update completes
        // This ensures audio continues without dropouts
    }

    float morphValue     = smoothValue (smMorph,     getParamValue (apvts, "zplane.morph"),     numSamples);
    float resonanceValue = smoothValue (smResonance, getParamValue (apvts, "zplane.resonance"), numSamples);
    float driveDb        = smoothValue (smDrive,     getParamValue (apvts, "drive.db"),         numSamples);
    float hardnessValue  = smoothValue (smHardness,  getParamValue (apvts, "hardness"),         numSamples);
    float mixParam = smoothValue (smMix,       getParamValue (apvts, "style.mix"),        numSamples);
    float tiltDb         = smoothValue (smTilt,      getParamValue (apvts, "tilt.brightness"),  numSamples);
    const bool safeModeEnabled = (getParamValue (apvts, "safe.mode") > 0.5f);
    const float trimLinear = smoothValue (smTrim, juce::Decibels::decibelsToGain (getParamValue (apvts, "output.trim")), numSamples);

    if (safeModeEnabled)
    {
        morphValue     = juce::jlimit (0.08f, 0.85f, morphValue);
        resonanceValue = juce::jlimit (0.0f, 0.50f, resonanceValue);
        driveDb        = juce::jmin (driveDb, 2.0f);
        hardnessValue  = juce::jmin (hardnessValue, 0.40f);
        tiltDb         = juce::jlimit (-2.0f, 2.0f, tiltDb);
        mixParam       = juce::jlimit (0.0f, 0.65f, mixParam);
    }

    if (safeModeEnabled != lastSafeModeState)
    {
        if (! safeModeEnabled)
            safeHighCut.reset();

        lastSafeModeState = safeModeEnabled;
    }

    emu.setMorphPosition (morphValue);
    emu.setIntensity (resonanceValue);
    emu.setDrive (driveDb);
    emu.setSectionSaturation (hardnessValue);

    const bool transparent = (resonanceValue <= 1.0e-4f)
                          && (std::abs (driveDb) < 1.0e-4f)
                          && (hardnessValue <= 1.0e-4f);

    tilt.setAmount (tiltDb);

    if (! transparent)
    {
        if (quality == Quality::Print && oversampler != nullptr)
        {
            juce::dsp::AudioBlock<float> inputBlock (buffer);
            auto osBlock = oversampler->processSamplesUp (inputBlock);

            const int osChannels = (int) osBlock.getNumChannels();
            const int osSamples  = (int) osBlock.getNumSamples();

            for (int ch = 0; ch < osChannels; ++ch)
                oversampleChannelPointers[(size_t) ch] = osBlock.getChannelPointer (ch);

            juce::AudioBuffer<float> osBuffer (oversampleChannelPointers.data(), osChannels, osSamples);

            emu.process (osBuffer);
            tilt.process (osBuffer);

            if (oversampleFactor > 1)
            {
                juce::dsp::AudioBlock<float> osAudioBlock (osBuffer);
                juce::dsp::ProcessContextReplacing<float> ctx (osAudioBlock);
                printLowpass.process (ctx);
            }

            oversampler->processSamplesDown (inputBlock);
        }
        else
        {
            emu.process (buffer);
            tilt.process (buffer);
        }
    }
    else if (tilt.isActive())
    {
        tilt.process (buffer);
    }

    publishFilterFrame();

    const float wetGain = eqPowerWet (mixParam);
    const float dryGain = eqPowerDry (mixParam);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* wet = buffer.getWritePointer (ch);
        auto* dry = dryScratch.getReadPointer (ch);
        for (int n = 0; n < numSamples; ++n)
            wet[n] = wetGain * wet[n] + dryGain * dry[n];
    }

    if (safeModeEnabled)
    {
        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        safeHighCut.process (ctx);
    }

    if (std::abs (trimLinear - 1.0f) > 1.0e-4f)
        buffer.applyGain (trimLinear);

    const float blockPeak = buffer.getMagnitude (0, numSamples);
    const float previousPeak = outputPeak.load();
    const float decayedPeak = previousPeak * 0.8f;
    outputPeak.store (juce::jmax (blockPeak, decayedPeak));

    if (blockPeak >= 0.995f)
        clipHold.store (clipHoldFrames);
    else
    {
        int current = clipHold.load();
        if (current > 0)
            clipHold.store (current - 1);
    }

    if (numChannels > 0)
        pushAnalysisSamples (buffer.getReadPointer (0), numSamples);
}

bool MorphEngineAudioProcessor::getLatestFilterFrame (FilterFrame& dest) const
{
    const auto seq = coeffSequence.load (std::memory_order_acquire);
    if (seq == 0)
        return false;

    dest = coeffFrames[(size_t) (seq & 1u)];

    return coeffSequence.load (std::memory_order_acquire) == seq;
}

void MorphEngineAudioProcessor::publishFilterFrame()
{
    auto seq = coeffSequence.load (std::memory_order_relaxed);
    const auto next = seq + 1u;
    auto& frame = coeffFrames[(size_t) (next & 1u)];
    emu.getSectionCoeffs (frame.coeffs);
    frame.poles = emu.getCurrentPoles();
    frame.morph = emu.getCurrentMorph();
    frame.intensity = emu.getCurrentIntensity();
    coeffSequence.store (next, std::memory_order_release);
}

//==============================================================================
void MorphEngineAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void MorphEngineAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (juce::AudioProcessor::getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

//==============================================================================
void MorphEngineAudioProcessor::initializeFactoryPresets()
{
    factoryPresets = {
        { "AIR:Freeze",    0, 0.18f, 0.52f,  0.45f, 1.2f, 0.28f, 0.30f, 0 },
        { "AIR:Glassline", 0, 0.20f, 0.50f,  0.50f, 1.4f, 0.30f, 0.32f, 0 },
        { "AIR:Whisper",   0, 0.16f, 0.46f,  0.42f, 1.0f, 0.26f, 0.28f, 0 },
        { "AIR:Halo",      0, 0.22f, 0.54f,  0.48f, 1.6f, 0.32f, 0.33f, 0 },
        { "AIR:Neon",      0, 0.24f, 0.56f,  0.55f, 1.8f, 0.34f, 0.35f, 0 },
        { "AIR:Vapor",     0, 0.17f, 0.48f,  0.44f, 1.3f, 0.28f, 0.30f, 0 },
        { "AIR:Breath",    0, 0.19f, 0.50f,  0.46f, 1.2f, 0.30f, 0.31f, 0 },
        { "AIR:Silk",      0, 0.21f, 0.49f,  0.43f, 1.1f, 0.29f, 0.29f, 0 },
        { "AIR:Shine",     0, 0.23f, 0.55f,  0.52f, 1.7f, 0.33f, 0.34f, 0 },
        { "AIR:Scenes",    0, 0.18f, 0.47f,  0.40f, 1.0f, 0.27f, 0.27f, 0 },
        { "AIR:Whitecap",  0, 0.20f, 0.53f,  0.51f, 1.5f, 0.31f, 0.33f, 0 },
        { "AIR:Ribbon",    0, 0.22f, 0.52f,  0.47f, 1.4f, 0.32f, 0.32f, 0 },

        { "LIQUID:Drift",    1, 0.22f, 0.40f,  0.08f, 2.2f, 0.34f, 0.36f, 0 },
        { "LIQUID:Shimmer",  1, 0.26f, 0.44f,  0.10f, 2.5f, 0.35f, 0.38f, 0 },
        { "LIQUID:Chrome",   1, 0.28f, 0.46f,  0.05f, 2.7f, 0.36f, 0.40f, 0 },
        { "LIQUID:Flux",     1, 0.24f, 0.43f,  0.06f, 2.3f, 0.34f, 0.37f, 0 },
        { "LIQUID:Phasewalk",1, 0.30f, 0.48f,  0.12f, 2.9f, 0.36f, 0.42f, 0 },
        { "LIQUID:Vellum",   1, 0.25f, 0.45f,  0.04f, 2.4f, 0.35f, 0.39f, 0 },
        { "LIQUID:Quartz",   1, 0.27f, 0.47f,  0.03f, 2.6f, 0.36f, 0.41f, 0 },
        { "LIQUID:Lantern",  1, 0.23f, 0.42f,  0.07f, 2.1f, 0.33f, 0.36f, 0 },
        { "LIQUID:Wavelet",  1, 0.29f, 0.45f,  0.09f, 2.8f, 0.37f, 0.43f, 0 },
        { "LIQUID:Glow",     1, 0.26f, 0.41f,  0.02f, 2.2f, 0.34f, 0.35f, 0 },
        { "LIQUID:Delta",    1, 0.24f, 0.44f,  0.01f, 2.0f, 0.33f, 0.37f, 0 },
        { "LIQUID:Opal",     1, 0.28f, 0.46f,  0.11f, 2.6f, 0.36f, 0.42f, 0 },

        { "PUNCH:Glue",   2, 0.14f, 0.32f, -0.40f, 3.0f, 0.40f, 0.28f, 0 },
        { "PUNCH:Snap",   2, 0.12f, 0.30f, -0.45f, 2.8f, 0.38f, 0.26f, 0 },
        { "PUNCH:Thump",  2, 0.16f, 0.34f, -0.35f, 3.2f, 0.42f, 0.30f, 0 },
        { "PUNCH:Pocket", 2, 0.13f, 0.31f, -0.38f, 2.9f, 0.39f, 0.27f, 0 },
        { "PUNCH:Crush",  2, 0.18f, 0.36f, -0.32f, 3.4f, 0.44f, 0.32f, 0 },
        { "PUNCH:Latch",  2, 0.11f, 0.29f, -0.42f, 2.7f, 0.37f, 0.25f, 0 },
        { "PUNCH:Lift",   2, 0.15f, 0.33f, -0.34f, 3.1f, 0.41f, 0.29f, 0 },
        { "PUNCH:Clamp",  2, 0.10f, 0.28f, -0.46f, 2.6f, 0.36f, 0.24f, 0 },
        { "PUNCH:Jaw",    2, 0.17f, 0.35f, -0.31f, 3.3f, 0.43f, 0.31f, 0 },
        { "PUNCH:Bar",    2, 0.13f, 0.30f, -0.37f, 2.8f, 0.38f, 0.26f, 0 },
        { "PUNCH:Body",   2, 0.15f, 0.34f, -0.33f, 3.1f, 0.41f, 0.30f, 0 },
        { "PUNCH:Edge",  2, 0.18f, 0.37f, -0.28f, 3.5f, 0.45f, 0.33f, 0 }
    };
}

juce::String MorphEngineAudioProcessor::getCurrentPresetName() const
{
    if (currentPresetIndex >= 0 && currentPresetIndex < (int) factoryPresets.size())
        return factoryPresets[(size_t) currentPresetIndex].name;
    return "Custom";
}

int MorphEngineAudioProcessor::getNumPresets() const
{
    return (int) factoryPresets.size();
}

juce::String MorphEngineAudioProcessor::getPresetName (int index) const
{
    if (index >= 0 && index < (int) factoryPresets.size())
        return factoryPresets[(size_t) index].name;
    return {};
}

void MorphEngineAudioProcessor::loadPreset (int index)
{
    if (index < 0 || index >= (int) factoryPresets.size())
        return;

    const auto& preset = factoryPresets[(size_t) index];

    auto set = [this](const juce::String& id, float value)
    {
        if (auto* param = apvts.getParameter (id))
        {
            param->beginChangeGesture();
            const auto norm = param->getNormalisableRange().convertTo0to1 (value);
            param->setValueNotifyingHost (norm);
            param->endChangeGesture();
        }
    };

    set ("style.variant",    (float) preset.style);
    set ("zplane.morph",      preset.morph);
    set ("zplane.resonance",  preset.resonance);
    set ("tilt.brightness",   preset.brightness);
    set ("drive.db",          preset.drive);
    set ("hardness",          preset.hardness);
    set ("style.mix",         preset.mix);
    set ("quality.mode",      (float) preset.quality);

    currentPresetIndex = index;
    lastStyleVariant.store (-1);
}

bool MorphEngineAudioProcessor::fillSpectrumSnapshot(float* dest, int numSamples)
{
    if (dest == nullptr || numSamples <= 0) return false;

    const juce::SpinLock::ScopedLockType lock (analysisLock);

    int valid = analysisValidSamples.load();
    if (valid < numSamples) return false;
    int w = analysisWritePos.load();
    int start = w - numSamples; if (start < 0) start += analysisBufferSize;
    for (int i = 0; i < numSamples; ++i)
        dest[i] = analysisBuffer[(start + i) % analysisBufferSize];
    return true;
}

void MorphEngineAudioProcessor::pushAnalysisSamples(const float* data, int numSamples)
{
    if (data == nullptr || numSamples <= 0) return;
    int w = analysisWritePos.load();
    for (int i = 0; i < numSamples; ++i)
        analysisBuffer[(w + i) % analysisBufferSize] = data[i];
    w = (w + numSamples) % analysisBufferSize;
    analysisWritePos.store(w);
    int v = analysisValidSamples.load();
    v = juce::jmin(analysisBufferSize, v + numSamples);
    analysisValidSamples.store(v);
}

float MorphEngineAudioProcessor::getOutputPeak() const noexcept
{
    return outputPeak.load();
}

bool MorphEngineAudioProcessor::isClipActive() const noexcept
{
    return clipHold.load() > 0;
}


//==============================================================================
juce::AudioProcessorEditor* MorphEngineAudioProcessor::createEditor()
{
    // Temporary runtime fallback: allow forcing a minimal editor if the host
    // or environment struggles to create the Premium UI (host-specific GPU/text backends etc.).
    // Set FE_SIMPLE_UI=1 in the environment to use the minimal terminal-style UI.
    if (std::getenv("FE_SIMPLE_UI") != nullptr)
        return new TerminalMorphUI (*this);

    return new PremiumMorphUI (*this);
}

void MorphEngineAudioProcessor::applyStyleMacro (int styleIndex)
{
    // Style changes only remap the underlying pole pairs; leave the user's
    // parameter balances untouched so the sound stays predictable.
    updateStyleState (styleIndex);
}

void MorphEngineAudioProcessor::handleAsyncUpdate()
{
    // Handle pending style changes
    const int styleIndex = pendingStyleMacro.exchange (-1);
    if (styleIndex >= 0)
        applyStyleMacro (styleIndex);

    // Handle pending quality changes
    QualityUpdateRequest qualityRequest;
    {
        const juce::SpinLock::ScopedLockType lock (qualityUpdateLock);
        if (pendingQualityUpdate.pending)
        {
            qualityRequest = pendingQualityUpdate;
            pendingQualityUpdate.pending = false;
        }
    }

    if (qualityRequest.pending)
    {
        currentBlockSize = qualityRequest.blockSize;
        configureQuality (qualityRequest.quality, qualityRequest.blockSize);
        initialiseSmoothers();
    }
}

void MorphEngineAudioProcessor::StyleParamListener::parameterChanged (const juce::String& paramID, float newValue)
{
    if (paramID == "style.variant")
    {
        processor.pendingStyleMacro.store ((int) std::round (newValue));
        processor.lastStyleVariant.store (-1);
        processor.triggerAsyncUpdate();
    }
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MorphEngineAudioProcessor();
}

