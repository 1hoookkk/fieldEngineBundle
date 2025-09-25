#include "PluginProcessor.h"
#include "PluginEditor.h"

PitchEngineAudioProcessor::PitchEngineAudioProcessor()
: juce::AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

bool PitchEngineAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getChannelSet (true,  0);
    const auto& out = layouts.getChannelSet (false, 0);
    return (in == out) && (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}

void PitchEngineAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    fs = sampleRate;
    // Use conservative upper bound for buffer allocation to prevent RT allocations
    const int maxChannels = juce::jmax(2, juce::jmax(getTotalNumInputChannels(), getTotalNumOutputChannels()));
    const int maxBlock = juce::jmax(samplesPerBlock, 2048); // Allow up to 2048 samples
    blockSize = maxBlock;

    zplane.prepare (fs);
    emuZPlane.prepare (fs);  // Initialize simplified EMU filter
    authenticEmu.prepare (fs); // Initialize authentic EMU filter

    // Setup new unified pitch engine
    pitchEngine.prepare (fs, maxBlock, 70.f, 800.f);
    pitchEngine.setKeyScale (0, 0x0FFF); // Default chromatic
    pitchEngine.setRetune (0.6f, 0); // Default retune settings

    // Setup dual-mode shifter
    const bool isTrackMode = (int(apvts.getRawParameterValue("qualityMode")->load()) == 0);
    shifter.prepare (fs, isTrackMode ? Shifter::TrackPSOLA : Shifter::PrintHQ);

    formantRescue.prepare (fs);
    formantRescue.setStyle (1); // Default Focus style

    // Initialize VoxZPlane brain
    voxBrain.prepare (fs, maxBlock, maxChannels);
    voxBrain.setMode (vox::Brain::Track); // Default Track mode
    voxBrain.setStyle (1); // Default Focus style

    // Wire AuthenticEMU instances to VoxZPlane providers
    // Using shared EMU instances for different processing stages
    voxBrain.wireEMUProviders(&authenticEmu, &authenticEmu, &authenticEmu);

    analyzer.prepare (fs, maxChannels);
    autoGain.reset (fs);

    // Pre-allocate buffers with generous capacity to prevent RT allocations
    dry.setSize (maxChannels, maxBlock, false, true, true);
    tmpMono.setSize (1, maxBlock, false, true, true);
    tmpMonoOut.setSize (1, maxBlock, false, true, true);
    tmpWetStereo.setSize (maxChannels, maxBlock, false, true, true);

    // Pre-allocate buffers for audio thread (avoid heap allocation)
    ratioBuf.assign((size_t)maxBlock, 1.0f);
    weightBuf.assign((size_t)maxBlock, 0.0f);
    limitedRatio.assign((size_t)maxBlock, 1.0f);

    // Smoothing
    styleSmoothed.reset    (fs, 0.05); // 50 ms
    strengthSmoothed.reset (fs, 0.10); // 100 ms
    retuneSmoothed.reset   (fs, 0.20); // 200 ms
    mixSmoothed.reset      (fs, 0.05); // 50 ms
    outputSmoothed.reset   (fs, 0.05); // 50 ms
    bypassXfade.reset      (fs, 0.010); // 10 ms click-safe bypass

    // Track mode (default) reports zero PDC
    reportedLatencySamples = 0;
    updateSnapperScaleFromParams();
}

void PitchEngineAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals _;

    // === 0) Numerics protection and input validation ===
    const int n = buffer.getNumSamples();
    const int numCh = juce::jmin (2, buffer.getNumChannels());

    // Validate buffer state
    if (n <= 0 || numCh <= 0) return;

    // RT-safety: Ensure we don't exceed pre-allocated capacity
    assertCapacity(n);

    // NaN/Inf protection on input buffer
    for (int ch = 0; ch < numCh; ++ch) {
        auto* channelData = buffer.getWritePointer(ch);
        for (int i = 0; i < n; ++i) {
            float& sample = channelData[i];
            if (!std::isfinite(sample)) {
                sample = 0.0f;  // Replace NaN/Inf with silence
            }
            // Additional denormal flush (belt and suspenders)
            else if (std::abs(sample) < 1e-15f) {
                sample = 0.0f;
            }
        }
    }

    // Params
    auto* pStyle    = apvts.getRawParameterValue ("style");
    auto* pStrength = apvts.getRawParameterValue ("strength");
    auto* pRetuneMs = apvts.getRawParameterValue ("retuneMs");
    auto* pBypass   = apvts.getRawParameterValue ("bypass");
    auto* pSecret   = apvts.getRawParameterValue ("secretMode");
    auto* pQual     = apvts.getRawParameterValue ("qualityMode");
    auto* pEngine   = apvts.getRawParameterValue ("processingEngine");

    // Report latency (Track=0, Print=48 ms) and notify host if changed
    const bool isTrackMode = (int(pQual->load()) == 0);
    const int newLatency = isTrackMode ? 0 : (int) std::round (fs * 0.048);

    if (newLatency != reportedLatencySamples) {
        reportedLatencySamples = newLatency;
        setLatencySamples(reportedLatencySamples); // Notify host of latency change
    }

    // Secret toggle
    const bool secret = (pSecret->load() > 0.5f);
    zplane.setSecretMode (secret);

    // Smooth incoming macro params
    styleSmoothed   .setTargetValue (juce::jlimit (0.0f, 100.0f, pStyle   ->load()));
    strengthSmoothed.setTargetValue (juce::jlimit (0.0f, 100.0f, pStrength->load()));
    retuneSmoothed  .setTargetValue (juce::jlimit (1.0f, 200.0f, pRetuneMs->load()));
    mixSmoothed     .setTargetValue (juce::jlimit (0.0f, 100.0f, apvts.getRawParameterValue("mix")->load()));
    outputSmoothed  .setTargetValue (juce::jlimit (-12.0f, 12.0f, apvts.getRawParameterValue("output")->load()));

    // Copy dry for crossfade using preallocated buffer
    dry.copyFrom (0, 0, buffer, 0, 0, n);
    if (numCh > 1)
        dry.copyFrom (1, 0, buffer, 1, 0, n);

    // === ENGINE SELECTION: Classic vs VoxZPlane vs A/B Test ===
    const int engineChoice = (int) pEngine->load();

    if (engineChoice == 1) // VoxZPlane
    {
        processVoxZPlane(buffer, pBypass, pQual, pStyle, pRetuneMs);
        return; // Skip classic pipeline
    }
    else if (engineChoice == 2) // A/B Test (split processing)
    {
        processABTest(buffer, pBypass, pQual, pStyle, pRetuneMs);
        return; // Skip classic pipeline
    }

    // === CLASSIC PIPELINE (engineChoice == 0) ===
    // === 1) Update pitch engine parameters ===
    updateSnapperScaleFromParams();
    const int key = (int) apvts.getRawParameterValue ("key")->load();
    const int scale = (int) apvts.getRawParameterValue ("scale")->load();
    const int biasChoice = (int) apvts.getRawParameterValue ("bias")->load();

    // Convert scale to bitmask
    uint16_t scaleMask = 0x0FFF; // Default chromatic
    if (scale == 1) scaleMask = 0x0AB5; // Major: C,D,E,F,G,A,B = bits 0,2,4,5,7,9,11
    if (scale == 2) scaleMask = 0x05AD; // Minor: C,D,Eb,F,G,Ab,Bb = bits 0,2,3,5,7,8,10

    // Convert bias choice to numeric bias (-1=Down, 0=Neutral, +1=Up)
    const int bias = (biasChoice == 0) ? -1 : (biasChoice == 2) ? +1 : 0;

    // Update pitch detection range from parameters
    const float rangeLow = apvts.getRawParameterValue ("rangeLow")->load();
    const float rangeHigh = apvts.getRawParameterValue ("rangeHigh")->load();
    pitchEngine.setRange (rangeLow, rangeHigh);

    pitchEngine.setKeyScale (key, scaleMask);
    pitchEngine.setRetune (1.0f - (retuneSmoothed.getNextValue() / 200.0f), bias);

    // === Classic Mode Configuration ===
    const bool classicMode = (pClassicMode->load() > 0.5f);
    pitchEngine.setClassicMode(classicMode);

    // === Classic Mode EMU Filter Preset ===
    if (classicMode) {
        const int filterStyle = (int) pClassicFilterStyle->load();
        // Apply classic EMU filter preset based on selected style
        switch (filterStyle) {
            case 0: // Velvet - Smooth, musical hardness reduction
                authenticEmu.setIntensity(0.35f);
                break;
            case 1: // Air - Bright, open character
                authenticEmu.setIntensity(0.65f);
                break;
            case 2: // Focus - Clear, defined snap
                authenticEmu.setIntensity(0.85f);
                break;
        }
    }

    // === 2) Unified pitch analysis (NSDF + per-sample ratios) ===
    PitchBlock pitchBlk;
    if (numCh > 0) {
        pitchBlk = pitchEngine.analyze (buffer.getReadPointer (0), n);
    }

    // === 3) Apply stabilizer to pitch data ===
    auto* pStab = apvts.getRawParameterValue ("stabilizer");
    const int stabIdx = int (pStab->load());
    const float holdMs = (stabIdx == 1 ? 40.0f : stabIdx == 2 ? 80.0f : stabIdx == 3 ? 200.0f : 0.0f);

    // Simple hold logic for stability
    const int holdN = int (fs * (holdMs / 1000.0f));
    if (holdN > 0 && pitchBlk.voiced) {
        const float currentMidi = 69.0f + 12.0f * std::log2(std::max(1e-9f, pitchBlk.f0 / 440.0f));
        const bool largeJump = std::abs(currentMidi - heldMidi) > 0.8f;
        if (largeJump) holdSamp = holdN;
        if (holdSamp > 0) { --holdSamp; } else { heldMidi = currentMidi; }
    }

    // === 4) Weight calculation ===
    const float strength01 = juce::jlimit (0.0f, 1.0f, strengthSmoothed.getNextValue() * 0.01f);
    const float mix01 = juce::jlimit (0.0f, 1.0f, mixSmoothed.getNextValue() * 0.01f);
    const float guardHF01 = juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue("guardHF")->load() * 0.01f);
    const float limitSemis = apvts.getRawParameterValue("limitSemis")->load();

    // Equal-power crossfade for better mix behavior
    const float wetGain = std::sin(mix01 * juce::MathConstants<float>::halfPi);
    const float dryGain = std::cos(mix01 * juce::MathConstants<float>::halfPi);

    float weight = strength01 * wetGain;

    // Advanced sibilant guard using guardHF parameter
    if (pitchBlk.sibilant) {
        const float sibilantReduction = 0.1f + 0.6f * (1.0f - guardHF01); // More guard = less reduction
        weight *= sibilantReduction;
    }

    // === 5) Dual-mode shifting ===
    shifter.setMode (isTrackMode ? Shifter::TrackPSOLA : Shifter::PrintHQ);

    // Build true mono tap for shifter (sum L+R if stereo)
    if (numCh > 1) {
        tmpMono.clear();
        tmpMono.addFrom(0, 0, buffer, 0, 0, n, 0.5f);
        tmpMono.addFrom(0, 0, buffer, 1, 0, n, 0.5f);
    } else if (numCh > 0) {
        tmpMono.copyFrom(0, 0, buffer, 0, 0, n);
    }

    // Apply semitone limiting to ratios into preallocated array
    for (int i = 0; i < n; ++i) {
        const float semis = 12.0f * std::log2(std::max(1e-6f, pitchBlk.ratio[i]));
        const float clampedSemis = juce::jlimit(-limitSemis, limitSemis, semis);
        limitedRatio[(size_t)i] = std::pow(2.0f, clampedSemis / 12.0f);
    }

    shifter.processBlock (tmpMono.getReadPointer(0), tmpMonoOut.getWritePointer(0), n, limitedRatio.data());

    // === 6) FormantRescue + Style/EMU Processing on WET BRANCH ONLY ===
    const float baseStyle01 = styleSmoothed.getNextValue() * 0.01f;

    // Build WET stereo from PSOLA output (no blend yet)
    juce::AudioBuffer<float> wetStereo (juce::jmax(2, numCh), n);
    for (int ch = 0; ch < wetStereo.getNumChannels(); ++ch)
        wetStereo.copyFrom(ch, 0, tmpMonoOut, 0, 0, n);

    // FormantRescue + Style on WET ONLY
    int styleMode = 1; if (baseStyle01 < 0.33f) styleMode = 0; else if (baseStyle01 >= 0.66f) styleMode = 2;
    formantRescue.setStyle(styleMode);

    if (secret) {
        // FormantRescue internally uses EMU, it must not touch 'buffer'
        formantRescue.processBlock(authenticEmu, limitedRatio.data(), n);

        // Tone shaping strictly on wet
        authenticEmu.setIntensity(juce::jlimit(0.0f, 1.0f, baseStyle01));
        authenticEmu.process(wetStereo); // <-- in-place, but only WET
    } else {
        const float finalStyle = juce::jlimit(0.0f, 1.0f, baseStyle01);
        if (finalStyle > 0.001f)
            zplane.process(wetStereo, finalStyle);
    }

    // === 7) Single final blend ===
    const float outGain = juce::Decibels::decibelsToGain(outputSmoothed.getNextValue());
    for (int ch = 0; ch < numCh; ++ch) {
        auto* out = buffer.getWritePointer(ch);
        const float* wet = wetStereo.getReadPointer(juce::jmin(ch, wetStereo.getNumChannels()-1));
        const float* dryData = dry.getReadPointer(ch);
        for (int i = 0; i < n; ++i)
            out[i] = outGain * (weight * wet[i] + dryGain * dryData[i]);
    }

    // === 8) Analyzer & meters ===
    if (numCh > 0) {
        analyzer.push (buffer.getReadPointer (0), n);
        analyzer.updatePitchData(pitchBlk.f0, pitchBlk.voiced ? 1.0f : 0.0f);
    }
    {
        float sumL=0.0f, sumR=0.0f, pkL=0.0f, pkR=0.0f;
        if (numCh > 0) {
            const float* d0 = buffer.getReadPointer (0);
            for (int i=0;i<n;++i){ const float v=d0[i]; sumL+=v*v; pkL=juce::jmax(pkL, std::abs(v)); }
        }
        if (numCh > 1) {
            const float* d1 = buffer.getReadPointer (1);
            for (int i=0;i<n;++i){ const float v=d1[i]; sumR+=v*v; pkR=juce::jmax(pkR, std::abs(v)); }
        }
        // Race-free meter update
        pushMeters(std::sqrt(sumL / juce::jmax(1, n)),
                   std::sqrt(sumR / juce::jmax(1, n)),
                   pkL >= 0.999f,
                   pkR >= 0.999f);
    }

    // === 9) AutoGain (honest A/B testing within ±0.5 dB) ===
    auto* pAutoG = apvts.getRawParameterValue ("autoGain");
    if (pAutoG->load() > 0.5f)
    {
        auto blockRMS = [](const float* d, int n) {
            double s = 0.0;
            for (int i = 0; i < n; ++i) { s += double(d[i]) * d[i]; }
            return float(std::sqrt(s / std::max(1, n)));
        };

        float rmsDry = 0.0f, rmsProc = 0.0f;
        if (numCh > 0) {
            rmsDry  = blockRMS (dry.getReadPointer(0), n);
            rmsProc = blockRMS (buffer.getReadPointer(0), n);
        }

        const float g = autoGain.compute (rmsProc, rmsDry);
        for (int ch = 0; ch < numCh; ++ch)
            buffer.applyGain (ch, 0, n, g);
    }

    // === 10) Click-safe bypass crossfade (10 ms) ===
    const float target = (pBypass->load() > 0.5f) ? 1.0f : 0.0f;
    bypassXfade.setTargetValue (target);
    for (int i=0;i<n;++i)
    {
        const float tX = bypassXfade.getNextValue(); // dry amount
        const float a  = 1.0f - tX;                   // processed amount
        if (numCh > 0){
            auto* out = buffer.getWritePointer (0);
            const auto* dr = dry.getReadPointer (0);
            out[i] = a * out[i] + tX * dr[i];
        }
        if (numCh > 1){
            auto* out = buffer.getWritePointer (1);
            const auto* dr = dry.getReadPointer (1);
            out[i] = a * out[i] + tX * dr[i];
        }
    }
}

void PitchEngineAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void PitchEngineAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorValueTreeState::ParameterLayout PitchEngineAudioProcessor::createLayout()
{
    using AP = juce::AudioProcessorValueTreeState;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    // === Core Parameters ===
    p.push_back (std::make_unique<juce::AudioParameterChoice> ("key", "Key",
        juce::StringArray { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" }, 9));
    p.push_back (std::make_unique<juce::AudioParameterChoice> ("scale", "Scale",
        juce::StringArray { "Chromatic","Major","Minor" }, 2));

    p.push_back (std::make_unique<juce::AudioParameterFloat> ("retuneMs", "Retune (ms)",
        juce::NormalisableRange<float> (1.0f, 200.0f, 0.01f), 12.0f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> ("strength", "Strength",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), 100.0f));

    // === New Parameters from Specification ===
    p.push_back (std::make_unique<juce::AudioParameterChoice> ("bias", "Bias",
        juce::StringArray { "Down","Neutral","Up" }, 1));

    p.push_back (std::make_unique<juce::AudioParameterFloat> ("mix", "Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), 100.0f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> ("output", "Output",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f));

    p.push_back (std::make_unique<juce::AudioParameterFloat> ("style", "Style",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), 35.0f));

    // === Advanced Parameters ===
    p.push_back (std::make_unique<juce::AudioParameterFloat> ("rangeLow", "Range Low (Hz)",
        juce::NormalisableRange<float> (60.0f, 200.0f, 0.1f), 70.0f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> ("rangeHigh", "Range High (Hz)",
        juce::NormalisableRange<float> (400.0f, 1200.0f, 0.1f), 800.0f));

    p.push_back (std::make_unique<juce::AudioParameterFloat> ("limitSemis", "Limit Semitones",
        juce::NormalisableRange<float> (1.0f, 24.0f, 0.1f), 12.0f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> ("guardHF", "Guard HF",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), 70.0f));

    // === Processing Engine Choice ===
    p.push_back (std::make_unique<juce::AudioParameterChoice> ("processingEngine", "Engine",
        juce::StringArray { "Classic", "VoxZPlane", "A/B Test" }, 0));

    // === Legacy Parameters ===
    p.push_back (std::make_unique<juce::AudioParameterFloat> ("formant", "Formant",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.01f), 80.0f));

    p.push_back (std::make_unique<juce::AudioParameterChoice> ("stabilizer", "Stabilizer",
        juce::StringArray { "Off","Short","Mid","Long" }, 0));

    p.push_back (std::make_unique<juce::AudioParameterChoice> ("qualityMode", "Quality",
        juce::StringArray { "Track","Print" }, 0));

    p.push_back (std::make_unique<juce::AudioParameterBool> ("autoGain", "Auto Gain", true));
    p.push_back (std::make_unique<juce::AudioParameterBool> ("bypass", "Bypass", false));
    p.push_back (std::make_unique<juce::AudioParameterBool> ("secretMode", "Mode X", false));

    return { p.begin(), p.end() };
}

void PitchEngineAudioProcessor::updateSnapperScaleFromParams()
{
    const int key   = (int) apvts.getRawParameterValue ("key")->load();
    const int scale = (int) apvts.getRawParameterValue ("scale")->load();
    snapper.setKey (key, scale);
}

float PitchEngineAudioProcessor::midiFromHz (float f0Hz, float lastMidi)
{
    if (f0Hz > 0.0f && std::isfinite(f0Hz) && f0Hz < 20000.0f) {
        const float ratio = f0Hz / 440.0f;
        if (ratio > 0.0f && std::isfinite(ratio)) {
            const float midiNote = 69.0f + 12.0f * std::log2(ratio);
            if (std::isfinite(midiNote) && midiNote >= 0.0f && midiNote <= 127.0f) {
                return midiNote;
            }
        }
    }
    return (std::isfinite(lastMidi) && lastMidi >= 0.0f && lastMidi <= 127.0f) ? lastMidi : 60.0f;
}

void PitchEngineAudioProcessor::processVoxZPlane(juce::AudioBuffer<float>& buffer,
                                                juce::AudioParameterFloat* pBypass,
                                                juce::AudioParameterFloat* pQual,
                                                juce::AudioParameterFloat* pStyle,
                                                juce::AudioParameterFloat* pRetuneMs)
{
    const bool bypass = (pBypass->load() > 0.5f);
    const bool isTrackMode = (int(pQual->load()) == 0);
    const float style01 = pStyle->load() * 0.01f;
    const float retuneMs = pRetuneMs->load();
    const float mix01 = mixSmoothed.getNextValue() * 0.01f;
    const float outputGain = juce::Decibels::decibelsToGain(outputSmoothed.getNextValue());

    int styleIndex = 1; if (style01 < 0.33f) styleIndex = 0; else if (style01 >= 0.66f) styleIndex = 2;

    voxBrain.setMode(isTrackMode ? vox::Brain::Track : vox::Brain::Print);
    voxBrain.setStyle(styleIndex);
    voxBrain.setUserMix(mix01);
    voxBrain.setRetuneMs(retuneMs);
    voxBrain.setBypass(bypass);

    voxBrain.process(buffer);

    if (outputGain != 1.0f) buffer.applyGain(outputGain);

    const int n = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();
    float sumL=0.0f, sumR=0.0f, pkL=0.0f, pkR=0.0f;
    if (numCh > 0) {
        const float* d0 = buffer.getReadPointer(0);
        for (int i=0;i<n;++i){ const float v=d0[i]; sumL+=v*v; pkL=juce::jmax(pkL, std::abs(v)); }
    }
    if (numCh > 1) {
        const float* d1 = buffer.getReadPointer(1);
        for (int i=0;i<n;++i){ const float v=d1[i]; sumR+=v*v; pkR=juce::jmax(pkR, std::abs(v)); }
    }
    // Race-free meter update
    pushMeters(std::sqrt(sumL / juce::jmax(1, n)),
               std::sqrt(sumR / juce::jmax(1, n)),
               pkL >= 0.999f,
               pkR >= 0.999f);
}

void PitchEngineAudioProcessor::processABTest(juce::AudioBuffer<float>& buffer,
                                             juce::AudioParameterFloat* pBypass,
                                             juce::AudioParameterFloat* pQual,
                                             juce::AudioParameterFloat* pStyle,
                                             juce::AudioParameterFloat* pRetuneMs)
{
    const int numSamples = buffer.getNumSamples();

    if (buffer.getNumChannels() >= 2) {
        juce::AudioBuffer<float> leftBuffer(1, numSamples);
        juce::AudioBuffer<float> rightBuffer(1, numSamples);

        leftBuffer.copyFrom(0, 0, buffer, 0, 0, numSamples);
        rightBuffer.copyFrom(0, 0, buffer, 0, 0, numSamples);

        juce::AudioBuffer<float> voxBuffer(2, numSamples);
        voxBuffer.copyFrom(0, 0, rightBuffer, 0, 0, numSamples);
        voxBuffer.copyFrom(1, 0, rightBuffer, 0, 0, numSamples);

        const bool bypass = (pBypass->load() > 0.5f);
        const bool isTrackMode = (int(pQual->load()) == 0);
        const float style01 = pStyle->load() * 0.01f;
        const float retuneMs = pRetuneMs->load();
        int styleIndex = style01 < 0.33f ? 0 : (style01 >= 0.66f ? 2 : 1);

        voxBrain.setMode(isTrackMode ? vox::Brain::Track : vox::Brain::Print);
        voxBrain.setStyle(styleIndex);
        voxBrain.setRetuneMs(retuneMs);
        voxBrain.setBypass(bypass);

        voxBrain.process(voxBuffer);

        buffer.copyFrom(0, 0, leftBuffer, 0, 0, numSamples);
        buffer.copyFrom(1, 0, voxBuffer, 0, 0, numSamples);
    }

    const int n = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();
    float sumL=0.0f, sumR=0.0f, pkL=0.0f, pkR=0.0f;
    if (numCh > 0) {
        const float* d0 = buffer.getReadPointer(0);
        for (int i=0;i<n;++i){ const float v=d0[i]; sumL+=v*v; pkL=juce::jmax(pkL, std::abs(v)); }
    }
    if (numCh > 1) {
        const float* d1 = buffer.getReadPointer(1);
        for (int i=0;i<n;++i){ const float v=d1[i]; sumR+=v*v; pkR=juce::jmax(pkR, std::abs(v)); }
    }
    // Race-free meter update
    pushMeters(std::sqrt(sumL / juce::jmax(1, n)),
               std::sqrt(sumR / juce::jmax(1, n)),
               pkL >= 0.999f,
               pkR >= 0.999f);
}

juce::AudioProcessorEditor* PitchEngineAudioProcessor::createEditor()
{
    return new PitchEngineEditor (*this);
}

void PitchEngineAudioProcessor::saveCurrentState(juce::ValueTree& targetState)
{
    targetState = apvts.copyState();
}

void PitchEngineAudioProcessor::recallState(const juce::ValueTree& sourceState)
{
    if (sourceState.isValid()) {
        apvts.replaceState(sourceState);
        updateSnapperScaleFromParams();
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PitchEngineAudioProcessor();
}