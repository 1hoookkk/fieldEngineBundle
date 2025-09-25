#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "libs/emu/api/IZPlaneEngine.h"

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
    blockSize = samplesPerBlock;

    // Initialize modular EMU engine
    const bool isTrackMode = (int(apvts.getRawParameterValue("qualityMode")->load()) == 0);
    emuEngine.prepare(fs, blockSize, juce::jmax(2, getTotalNumOutputChannels()));
    auto osMode = isTrackMode ? OversampledEngine::Mode::Off_1x : OversampledEngine::Mode::OS2_IIR;
    osEmu.prepare(fs, juce::jmax(2, getTotalNumOutputChannels()), osMode);
    osEmu.setMaxBlock(blockSize);

    // Setup new unified pitch engine
    pitchEngine.prepare (fs, blockSize, 70.f, 800.f);
    pitchEngine.setKeyScale (0, 0x0FFF); // Default chromatic
    pitchEngine.setRetune (0.6f, 0); // Default retune settings

    // Setup dual-mode shifter (reuse isTrackMode from oversampling setup)
    shifter.prepare (fs, isTrackMode ? Shifter::TrackPSOLA : Shifter::PrintHQ);

    // Note: FormantRescue and VoxZPlane are replaced by integrated formant handling in AuthenticEMUEngine

    analyzer.prepare (fs, getTotalNumInputChannels());
    autoGain.reset (fs);

    dry.setSize (getTotalNumInputChannels(), blockSize, false, true, true);
    tmpMono.setSize (1, blockSize, false, true, true);
    tmpMonoOut.setSize (1, blockSize, false, true, true);
    tmpWetStereo.setSize (juce::jmax(2, getTotalNumOutputChannels()), blockSize, false, true, true);

    // Pre-allocate buffers for audio thread (avoid heap allocation)
    ratioBuf.assign((size_t)blockSize, 1.0f);
    weightBuf.assign((size_t)blockSize, 0.0f);
    limitedRatio.assign((size_t)blockSize, 1.0f);

    // Smoothing
    styleSmoothed.reset    (fs, 0.05); // 50 ms
    strengthSmoothed.reset (fs, 0.10); // 100 ms
    retuneSmoothed.reset   (fs, 0.20); // 200 ms
    mixSmoothed.reset      (fs, 0.05); // 50 ms
    outputSmoothed.reset   (fs, 0.05); // 50 ms
    bypassXfade.reset      (fs, 0.010); // 10 ms click-safe bypass

    reportedLatencySamples = osEmu.getLatencySamples();
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

    // Ensure temp buffers sized if host changed block size dynamically
    if (n > blockSize)
    {
        blockSize = n;
        dry.setSize (getTotalNumOutputChannels(), blockSize, false, true, true);
        tmpMono.setSize (1, blockSize, false, true, true);
        tmpMonoOut.setSize (1, blockSize, false, true, true);
        tmpWetStereo.setSize (juce::jmax(2, getTotalNumOutputChannels()), blockSize, false, true, true);
        ratioBuf.assign((size_t)blockSize, 1.0f);
        weightBuf.assign((size_t)blockSize, 0.0f);
        limitedRatio.assign((size_t)blockSize, 1.0f);
    }

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
    auto* pClassicMode = apvts.getRawParameterValue ("classic_mode");
    auto* pClassicFilterStyle = apvts.getRawParameterValue ("classic_filter_style");

    // Report latency and switch oversampling mode if changed
    const bool isTrackMode = (int(pQual->load()) == 0);
    const auto desiredMode = isTrackMode ? OversampledEngine::Mode::Off_1x : OversampledEngine::Mode::OS2_IIR;

    // Latency discipline: report every block
    reportedLatencySamples = osEmu.getLatencySamples();
    setLatencySamples(reportedLatencySamples);

    // Secret mode for advanced processing (now handled by EMU engine)
    const bool secret = (pSecret->load() > 0.5f);

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

    // === CONSOLIDATED MODULAR PIPELINE ===
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

    float weight = strength01 * mix01;

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
        float r = pitchBlk.ratio[i];
        if (!std::isfinite(r)) r = 1.0f;
        r = std::clamp(r, 0.25f, 4.0f);

        const float semis = 12.0f * std::log2(std::max(1e-6f, r));
        const float clampedSemis = juce::jlimit(-limitSemis, limitSemis, semis);
        limitedRatio[(size_t)i] = std::pow(2.0f, clampedSemis / 12.0f);
    }

    shifter.processBlock (tmpMono.getReadPointer(0), tmpMonoOut.getWritePointer(0), n, limitedRatio.data(), pitchBlk.f0, pitchBlk.voiced ? 1.0f : 0.0f);

    // === 6) FormantRescue + Style/EMU Processing on WET BRANCH ONLY ===
    const float baseStyle = styleSmoothed.getNextValue() * 0.01f;

    // Prepare wet stereo from mono shifter output (WET ONLY)
    tmpWetStereo.clear();
    for (int ch = 0; ch < tmpWetStereo.getNumChannels(); ++ch)
        tmpWetStereo.copyFrom(ch, 0, tmpMonoOut, 0, 0, n);

    // Style mode is now handled by the EMU engine morph parameters

    // PRIMARY MODULAR EMU ENGINE: Comprehensive APVTS → ZPlaneParams mapping
    ZPlaneParams zpar{};

    // Core morphing parameters from dedicated Z-plane controls
    zpar.morphPair = (int) apvts.getRawParameterValue("z_morph_pair")->load();
    zpar.morph = apvts.getRawParameterValue("z_morph")->load();
    zpar.intensity = apvts.getRawParameterValue("z_intensity")->load();

    // Drive and saturation from dedicated controls
    zpar.driveDb = apvts.getRawParameterValue("z_drive_db")->load();
    zpar.sat = apvts.getRawParameterValue("z_sat")->load();

    // Advanced EMU parameters now exposed through UI
    zpar.radiusGamma = apvts.getRawParameterValue("z_radius_gamma")->load();
    zpar.postTiltDbPerOct = apvts.getRawParameterValue("z_post_tilt")->load();
    zpar.driveHardness = apvts.getRawParameterValue("z_drive_hard")->load();
    zpar.lfoRate = 0.0f;
    zpar.lfoDepth = 0.0f;
    zpar.autoMakeup = true;

    // Formant-pitch coupling using proper ZPlaneParams fields
    const int formantModeChoice = (int) apvts.getRawParameterValue("z_formant_mode")->load();
    zpar.formantLock = (formantModeChoice == 0); // 0 = Lock, 1 = Follow

    // Compute average pitch ratio for formant compensation
    double avgRatio = 0.0;
    for (int i = 0; i < n; ++i) {
        avgRatio += limitedRatio[(size_t)i];
    }
    zpar.pitchRatio = float(avgRatio / std::max(1, n));

    // Update section count based on parameter
    const int sectionsChoice = (int) apvts.getRawParameterValue("z_sections")->load();
    const int sectionsActive = (sectionsChoice == 0) ? 3 : 6; // 6th order = 3 sections, 12th order = 6 sections
    emuEngine.setSectionsActive(sectionsActive);

    // Apply parameters and process through modular engine
    emuEngine.setParams(zpar);
    if (!emuEngine.isEffectivelyBypassed()) {
        osEmu.process(emuEngine, tmpWetStereo, n);
    }

    // === 7) Clean wet-only blend: dry + wet * weight, then output gain ===
    const float outGain = juce::Decibels::decibelsToGain(outputSmoothed.getNextValue());
    // Zero-latency tracking path: wet-only to avoid comb with the DAW’s direct input
    if (isTrackMode) {
        for (int ch = 0; ch < numCh; ++ch) {
            auto* out = buffer.getWritePointer(ch);
            const float* wetPtr = tmpWetStereo.getReadPointer(juce::jmin(ch, tmpWetStereo.getNumChannels()-1));
            for (int i=0; i<n; ++i)
                out[i] = outGain * wetPtr[i];
        }
    } else {
        for (int ch = 0; ch < numCh; ++ch) {
            auto* out = buffer.getWritePointer(ch);
            const float* dryPtr = dry.getReadPointer(juce::jmin(ch, dry.getNumChannels()-1));
            const float* wetPtr = tmpWetStereo.getReadPointer(juce::jmin(ch, tmpWetStereo.getNumChannels()-1));

            for (int i = 0; i < n; ++i) {
                // Clean blend: (1-weight)*dry + weight*wet, then apply output gain
                const float blended = (1.0f - weight) * dryPtr[i] + weight * wetPtr[i];
                out[i] = outGain * blended;
            }
        }
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
        meters.rmsL.store (std::sqrt (sumL / juce::jmax (1, n)));
        meters.rmsR.store (std::sqrt (sumR / juce::jmax (1, n)));
        meters.clipL.store (pkL >= 0.999f);
        meters.clipR.store (pkR >= 0.999f);
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


    // === Z-Plane EMU Parameters ===
    p.push_back (std::make_unique<juce::AudioParameterChoice> ("z_morph_pair", "Z-Plane Pair",
        juce::StringArray { "Vowel", "Bell", "Low", "Lead", "Pad" }, 0));
    p.push_back (std::make_unique<juce::AudioParameterFloat> ("z_morph", "Z-Plane Morph",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> ("z_intensity", "Z-Plane Intensity",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.35f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> ("z_drive_db", "Z-Plane Drive",
        juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> ("z_sat", "Z-Plane Saturation",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> ("z_radius_gamma", "Z-Plane Radius Gamma",
        juce::NormalisableRange<float> (0.8f, 1.2f, 0.001f), 1.0f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> ("z_post_tilt", "Z-Plane Post Tilt",
        juce::NormalisableRange<float> (-3.0f, 3.0f, 0.1f), 0.0f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> ("z_drive_hard", "Z-Plane Drive Hardness",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    p.push_back (std::make_unique<juce::AudioParameterChoice> ("z_sections", "Z-Plane Sections",
        juce::StringArray { "6th Order", "12th Order" }, 1));
    p.push_back (std::make_unique<juce::AudioParameterChoice> ("z_formant_mode", "Formant Mode",
        juce::StringArray { "Lock", "Follow" }, 0));

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

    // === Classic Mode Parameters ===
    p.push_back (std::make_unique<juce::AudioParameterBool> ("classic_mode", "Classic Mode", true));
    p.push_back (std::make_unique<juce::AudioParameterChoice> ("classic_filter_style", "Classic Filter",
        juce::StringArray { "Velvet","Air","Focus" }, 0));

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
                                                std::atomic<float>* pBypass,
                                                std::atomic<float>* pQual,
                                                std::atomic<float>* pStyle,
                                                std::atomic<float>* pRetuneMs)
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
    meters.rmsL.store(std::sqrt(sumL / juce::jmax(1, n)));
    meters.rmsR.store(std::sqrt(sumR / juce::jmax(1, n)));
    meters.clipL.store(pkL >= 0.999f);
    meters.clipR.store(pkR >= 0.999f);
}

void PitchEngineAudioProcessor::processABTest(juce::AudioBuffer<float>& buffer,
                                             std::atomic<float>* pBypass,
                                             std::atomic<float>* pQual,
                                             std::atomic<float>* pStyle,
                                             std::atomic<float>* pRetuneMs)
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
    meters.rmsL.store(std::sqrt(sumL / juce::jmax(1, n)));
    meters.rmsR.store(std::sqrt(sumR / juce::jmax(1, n)));
    meters.clipL.store(pkL >= 0.999f);
    meters.clipR.store(pkR >= 0.999f);
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

// --- helpers --------------------------------------------------------------
void PitchEngineAudioProcessor::setParamValue(const juce::String& id, float realValue)
{
    if (auto* p = apvts.getParameter(id))
    {
        auto range = apvts.getParameterRange(id);                   // NormalisableRange<float>
        const float norm = range.convertTo0to1(range.snapToLegalValue(realValue));
        p->beginChangeGesture();
        p->setValueNotifyingHost(norm);
        p->endChangeGesture();
    }
}

int PitchEngineAudioProcessor::getParamInt(const juce::String& id) const
{
    // For choice/int params — use the *current* value as integer
    if (auto* p = apvts.getParameter(id))
    {
        auto range = apvts.getParameterRange(id);
        const float real = range.convertFrom0to1(p->getValue());
        return (int) std::round(real);
    }
    return 0;
}

// --- HARD-TUNE macro ------------------------------------------------------
void PitchEngineAudioProcessor::applyHardTunePreset(bool formantFollow)
{
    // Parameter IDs used below — adjust if your IDs differ
    // (these match what we discussed earlier)
    constexpr auto ID_KEY          = "key";           // 0..11 (C..B)
    constexpr auto ID_SCALE        = "scale";         // 0=Chrom,1=Major,2=Minor
    constexpr auto ID_RETUNE_MS    = "retuneMs";     // 1..200 ms
    constexpr auto ID_STRENGTH     = "strength";      // 0..1
    constexpr auto ID_BIAS         = "bias";          // 0=Down,1=Neutral,2=Up
    constexpr auto ID_QUALITY      = "qualityMode";  // 0=Track,1=Print
    constexpr auto ID_FORMANT_MODE = "z_formant_mode";  // 0=Lock,1=Follow

    // Keep user’s current key/scale if already set:
    const int curKey   = getParamInt(ID_KEY);
    const int curScale = getParamInt(ID_SCALE);

    // Core “T-Pain / Evo” moves:
    setParamValue(ID_QUALITY,      0);        // Track / zero-latency (PSOLA)
    setParamValue(ID_RETUNE_MS,    3.0f);     // super fast retune
    setParamValue(ID_STRENGTH,     100.0f);     // 100%
    setParamValue(ID_BIAS,         2);        // Up bias
    setParamValue(ID_FORMANT_MODE, formantFollow ? 1 : 0); // Follow=robotic classic

    // Preserve (or force) musical context:
    setParamValue(ID_KEY,          (float)curKey);
    setParamValue(ID_SCALE,        (float)curScale);

    // Safety: if you expose Z-plane/OS params, make sure Track path is zero-latency
    // (only needed if you accidentally left Print/OS enabled)
    // setParamValue("z_os_mode", 0); // Off 1x — uncomment if applicable
}


juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PitchEngineAudioProcessor();
}