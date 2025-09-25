#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
FieldEngineAudioProcessor::FieldEngineAudioProcessor()
     : AudioProcessor (BusesProperties()
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                       ),
       apvts(*this, nullptr, "PARAMS", createParameterLayout())
{
    // Add parameter listeners to sync APVTS with DSP engine
    apvts.addParameterListener("t1", this);
    apvts.addParameterListener("t2", this);
    apvts.addParameterListener("cutoff", this);
    apvts.addParameterListener("resonance", this);
    apvts.addParameterListener("model", this);

    // Add Chordifier parameter listeners
    apvts.addParameterListener("mode", this);
    apvts.addParameterListener("key", this);
    apvts.addParameterListener("scale", this);
    apvts.addParameterListener("chord", this);
    apvts.addParameterListener("q", this);
    apvts.addParameterListener("drywet", this);

    // Add Z-Plane morphing parameter listeners
    apvts.addParameterListener("drive", this);
    apvts.addParameterListener("intensity", this);
    apvts.addParameterListener("morph", this);
    apvts.addParameterListener("autoMakeup", this);
    apvts.addParameterListener("sectSat", this);
    apvts.addParameterListener("satAmt", this);

    // Built-in LFO params
    apvts.addParameterListener("lfoRate", this);
    apvts.addParameterListener("lfoDepth", this);
    apvts.addParameterListener("lfoStereo", this);

    // Env→Morph
    apvts.addParameterListener("envDepth", this);
    apvts.addParameterListener("envAttack", this);
    apvts.addParameterListener("envRelease", this);
    apvts.addParameterListener("envInvert", this);

    // Blend and theme parameters
    apvts.addParameterListener("blend", this);
    apvts.addParameterListener("themeHue", this);
    apvts.addParameterListener("themeSat", this);
    apvts.addParameterListener("themeVal", this);
    
    // Hub parameters
    apvts.addParameterListener("hubBypass", this);
    apvts.addParameterListener("hubSoloWet", this);
    apvts.addParameterListener("hubAudition", this);
    apvts.addParameterListener("hubAuditionType", this);
    apvts.addParameterListener("hubAuditionLevel", this);
    apvts.addParameterListener("midiLocal", this);
    apvts.addParameterListener("serial", this);
}

FieldEngineAudioProcessor::~FieldEngineAudioProcessor()
{
}

//==============================================================================
const juce::String FieldEngineAudioProcessor::getName() const
{
    return "fieldEngine";
}

bool FieldEngineAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool FieldEngineAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool FieldEngineAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double FieldEngineAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int FieldEngineAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int FieldEngineAudioProcessor::getCurrentProgram()
{
    return 0;
}

void FieldEngineAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String FieldEngineAudioProcessor::getProgramName (int index)
{
    return {};
}

void FieldEngineAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void FieldEngineAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::FloatVectorOperations::enableFlushToZeroMode (true);
    juce::FloatVectorOperations::disableDenormalisedNumberSupport (true);

    // Safety: ignore invalid prepare calls
    if (sampleRate <= 0.0 || samplesPerBlock <= 0)
    {
        engineInitialized.store(false);
        return;
    }

    // Initialize DSP engine and Chordifier
    dspEngine.prepare(sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    chordifier.prepare(sampleRate, samplesPerBlock);

    // Load Audity 2000 bank for authentic EMU morphing
    if (dspEngine.loadAudityBank()) {
        DBG("Audity 2000 bank loaded successfully with " << dspEngine.getNumAudityPresets() << " presets");
    } else {
        DBG("Audity 2000 bank not found - using default Z-plane shapes");
    }

    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;
    panicMute = false; // Reset panic state

    tempDry.setSize(getTotalNumOutputChannels(), samplesPerBlock, false, false, true);

    // Initialize parallel routing buffers
    tempFilter.setSize(getTotalNumOutputChannels(), samplesPerBlock, false, false, true);
    tempChord.setSize(getTotalNumOutputChannels(), samplesPerBlock, false, false, true);

    // Init LFOs from current params - with null safety checks
    lfoL.prepare(sampleRate);
    
    // Initialize test tone generator
    testTone.prepare(sampleRate);
    
    // Initialize recording buffer
    recordBuffer.setSize(2, samplesPerBlock, false, false, true);
    lfoR.prepare(sampleRate);
    
    // SAFE parameter access with null checks
    auto* rateParam = apvts.getRawParameterValue("lfoRate");
    auto* depthParam = apvts.getRawParameterValue("lfoDepth");
    auto* stereoParam = apvts.getRawParameterValue("lfoStereo");
    auto* morphParam = apvts.getRawParameterValue("morph");
    auto* atkParam = apvts.getRawParameterValue("envAttack");
    auto* relParam = apvts.getRawParameterValue("envRelease");
    auto* depthEnvParam = apvts.getRawParameterValue("envDepth");
    auto* invertParam = apvts.getRawParameterValue("envInvert");
    
    const float rate   = rateParam ? rateParam->load() : 0.20f;
    const float depth  = depthParam ? depthParam->load() : 0.25f;
    const float stereo = stereoParam ? stereoParam->load() : 90.0f;
    
    lfoL.setRateHz(rate);
    lfoR.setRateHz(rate);
    lfoL.setPhaseOffset(0.0f);
    lfoR.setPhaseOffset(stereo);
    lfoDepth = depth;
    baseMorph = morphParam ? morphParam->load() : 0.0f;

    // Prepare Env→Morph follower with safe parameter access
    const float atk = atkParam ? atkParam->load() : 5.0f;
    const float rel = relParam ? relParam->load() : 120.0f;
    envMod.prepare(sampleRate, atk, rel);
    envMod.depth  = depthEnvParam ? depthEnvParam->load() : 0.35f;
    envMod.invert = invertParam ? (invertParam->load() > 0.5f) : false;

    // Load default Z-plane shapes to ensure DSP engine has valid data
    loadDefaultShapes();

    // Force parameter synchronization to ensure all DSP values are set
    forceParameterSync();

    engineInitialized = true; // Mark engine as ready for parameter updates
}

void FieldEngineAudioProcessor::releaseResources()
{
    // TODO: engine.release();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool FieldEngineAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Only mono/stereo supported
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // Input and output should have same channel count
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}
#endif

void FieldEngineAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    if (numSamples <= 0 || !engineInitialized.load()) {
        buffer.clear();
        return;
    }
    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    if (totalNumOutputChannels <= 0) {
        return;
    }

    // Clear unused output channels first
    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    // 1. MIDI Events Processing (for audition tone control)
    const int midiReady = midiEventFifo.getNumReady();
    if (midiReady > 0) {
        const auto scope = midiEventFifo.read(midiReady);
        for (int i = 0; i < scope.blockSize1; ++i) {
            const auto& evt = midiEventBuffer[scope.startIndex1 + i];
            if (evt.isNoteOn) {
                testTone.setFrequency(440.0f * std::pow(2.0f, (evt.noteNumber - 69) / 12.0f));
            }
        }
        for (int i = 0; i < scope.blockSize2; ++i) {
            const auto& evt = midiEventBuffer[scope.startIndex2 + i];
            if (evt.isNoteOn) {
                testTone.setFrequency(440.0f * std::pow(2.0f, (evt.noteNumber - 69) / 12.0f));
            }
        }
    }

    // 2. Test Tone Generation (additive mixing if audition active)
    auto* auditionParam = apvts.getRawParameterValue("hubAudition");
    if (auditionParam && auditionParam->load() > 0.5f) {
        const int auditionType = static_cast<int>((apvts.getRawParameterValue("hubAuditionType") ? apvts.getRawParameterValue("hubAuditionType")->load() : 0.0f));
        const float auditionLevel = (apvts.getRawParameterValue("hubAuditionLevel") ? apvts.getRawParameterValue("hubAuditionLevel")->load() : -12.0f);
        
        float* leftChannel = buffer.getWritePointer(0);
        float* rightChannel = totalNumOutputChannels > 1 ? buffer.getWritePointer(1) : leftChannel;
        
        testTone.render(leftChannel, rightChannel, numSamples, totalNumOutputChannels, auditionType, auditionLevel);
    }

    // 3. Store dry signal (for both solo wet and safety fallback)
    tempDry.setSize(totalNumOutputChannels, numSamples, false, false, true);
    for (int ch = 0; ch < totalNumOutputChannels; ++ch)
    {
        const int srcCh = ch < totalNumInputChannels ? ch : 0; // mono->stereo safety
        tempDry.copyFrom(ch, 0, buffer, srcCh, 0, numSamples);
    }

    // 4. Bypass Check (early exit but still record)
    const bool bypassEnabled = (apvts.getRawParameterValue("hubBypass") && apvts.getRawParameterValue("hubBypass")->load() > 0.5f);
    if (bypassEnabled) {
        // Record dry signal if recording
        if (isRecording.load() && wavWriter) {
            recordBuffer.setSize(2, numSamples, false, false, true);
            for (int ch = 0; ch < juce::jmin(2, totalNumOutputChannels); ++ch) {
                recordBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);
            }
            wavWriter->writeFromAudioSampleBuffer(recordBuffer, 0, numSamples);
        }
        return; // True bypass - return unprocessed
    }

    // Update host tempo for tempo-sync LFOs (RT-safe)
    updateHostTempo();

    // Check for thread-safe shape updates (RT-safe atomic read)
    int globalBankIndex = audioShapeBankIndex.load();
    if (globalBankIndex != audioLocalBankIndex) {
        // New shapes available - copy to DSP engine
        dspEngine.setShapeA(audioShapeA_banks[globalBankIndex]);
        dspEngine.setShapeB(audioShapeB_banks[globalBankIndex]);
        audioLocalBankIndex = globalBankIndex; // Update local copy
    }

    // Check panic mute flag (safety)
    if (panicMute.load()) {
        // passthrough dry to avoid silence while muted
        for (int ch = 0; ch < totalNumOutputChannels; ++ch)
            buffer.copyFrom(ch, 0, tempDry, ch, 0, numSamples);
        return;
    }

    // 5. EXISTING MORPHING ENGINE - Process audio through appropriate engine
    // Check processing mode: 0 = Z-plane only, 1 = Chordifier only, 2 = Both
    int mode = static_cast<int>((apvts.getRawParameterValue("mode") ? apvts.getRawParameterValue("mode")->load() : 2.0f));
    float* leftChannel = buffer.getWritePointer(0);
    float* rightChannel = totalNumOutputChannels > 1 ? buffer.getWritePointer(1) : leftChannel;

    // Get blend parameter for parallel processing
    const float blend = (apvts.getRawParameterValue("blend") ? apvts.getRawParameterValue("blend")->load() : 0.5f);
    const bool isSerial = (apvts.getRawParameterValue("serial") && apvts.getRawParameterValue("serial")->load() > 0.5f);

    if (mode == 0) {
        // ---- Filter Only: Z-plane with built-in LFO + Env in chunks ----
        for (int i = 0; i < numSamples; i += kModChunk)
        {
            const int n = std::min (kModChunk, numSamples - i);
            // LFO contribution (left channel reference)
            const float lfoNow = lfoEnabled ? lfoL.tick() : 0.0f;     // [-1..+1]
            const float lfoOff = (lfoDepth * 0.5f) * (lfoNow + 1.0f) - (lfoDepth * 0.5f); // [-d/2..+d/2]

            // Envelope from dry input (mono mix) over this chunk
            float envNow = 0.0f;
            const float* dL = tempDry.getReadPointer(0) + i;
            const float* dR = (tempDry.getNumChannels()>1 ? tempDry.getReadPointer(1) : dL) + i;
            for (int s = 0; s < n; ++s) {
                const float mono = 0.5f * (dL[s] + dR[s]);
                envNow = envMod.processSample(mono); // AR follower per sample
            }
            const float envOff = envMod.depth * (envNow - 0.5f);      // center around 0, scale depth

            const float morph = juce::jlimit(0.0f, 1.0f, baseMorph + lfoOff + envOff);
            dspEngine.setMorph(morph);
            dspEngine.processBlock (leftChannel + i, rightChannel + i, n);
        }
    } else if (mode == 1) {
        // ---- Chordifier Only ----
        chordifier.process(leftChannel, rightChannel, numSamples);
    } else {
        // ---- Filter + Chordifier: Parallel or Serial processing ----
        if (isSerial) {
            // Serial: Filter -> Chordifier
            for (int i = 0; i < numSamples; i += kModChunk) {
                const int n = std::min (kModChunk, numSamples - i);
                const float lfoNow = lfoEnabled ? lfoL.tick() : 0.0f;
                const float lfoOff = (lfoDepth * 0.5f) * (lfoNow + 1.0f) - (lfoDepth * 0.5f);

                float envNow = 0.0f;
                const float* dL = tempDry.getReadPointer(0) + i;
                const float* dR = (tempDry.getNumChannels()>1 ? tempDry.getReadPointer(1) : dL) + i;
                for (int s = 0; s < n; ++s) {
                    const float mono = 0.5f * (dL[s] + dR[s]);
                    envNow = envMod.processSample(mono);
                }
                const float envOff = envMod.depth * (envNow - 0.5f);
                const float morph = juce::jlimit(0.0f, 1.0f, baseMorph + lfoOff + envOff);
                dspEngine.setMorph(morph);
                dspEngine.processBlock (leftChannel + i, rightChannel + i, n);
            }
            // Then chordifier processes the filtered output
            chordifier.process(leftChannel, rightChannel, numSamples);
        } else {
            // Parallel: blend between Filter and Chordifier
            // Prepare parallel buffers
            tempFilter.makeCopyOf(buffer, true);
            tempChord.makeCopyOf(buffer, true);

            // Process Filter path
            {
                auto* l = tempFilter.getWritePointer(0);
                auto* r = tempFilter.getNumChannels() > 1 ? tempFilter.getWritePointer(1) : l;

                for (int i = 0; i < numSamples; i += kModChunk) {
                    const int n = std::min (kModChunk, numSamples - i);
                    const float lfoNow = lfoEnabled ? lfoL.tick() : 0.0f;
                    const float lfoOff = (lfoDepth * 0.5f) * (lfoNow + 1.0f) - (lfoDepth * 0.5f);

                    float envNow = 0.0f;
                    const float* dL = tempDry.getReadPointer(0) + i;
                    const float* dR = (tempDry.getNumChannels()>1 ? tempDry.getReadPointer(1) : dL) + i;
                    for (int s = 0; s < n; ++s) {
                        const float mono = 0.5f * (dL[s] + dR[s]);
                        envNow = envMod.processSample(mono);
                    }
                    const float envOff = envMod.depth * (envNow - 0.5f);
                    const float morph = juce::jlimit(0.0f, 1.0f, baseMorph + lfoOff + envOff);
                    dspEngine.setMorph(morph);
                    dspEngine.processBlock (l + i, r + i, n);
                }
            }

            // Process Chordifier path
            {
                auto* l = tempChord.getWritePointer(0);
                auto* r = tempChord.getNumChannels() > 1 ? tempChord.getWritePointer(1) : l;
                chordifier.process(l, r, numSamples);
            }

            // Equal-power crossfade: out = (1-blend)*Filter + blend*Chordifier
            const float a = std::sqrt(1.0f - blend);
            const float b = std::sqrt(blend);

            for (int ch = 0; ch < totalNumOutputChannels; ++ch) {
                auto* dest = buffer.getWritePointer(ch);
                const float* filterSrc = tempFilter.getReadPointer(ch);
                const float* chordSrc = tempChord.getReadPointer(ch);

                for (int i = 0; i < numSamples; ++i) {
                    dest[i] = a * filterSrc[i] + b * chordSrc[i];
                }
            }
        }
    }

    // 6. Solo Wet Processing
    const bool soloWetEnabled = apvts.getRawParameterValue("hubSoloWet")->load() > 0.5f;
    if (!soloWetEnabled) {
        // Normal operation: already contains processed signal (wet)
        // Could add dry/wet mix here if needed
    }
    // If solo wet is enabled, buffer already contains only wet signal

    // NaN/Inf safety scrubbing
    for (int ch = 0; ch < totalNumOutputChannels; ++ch)
    {
        auto* d = buffer.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i)
        {
            float v = d[i];
            if (!std::isfinite(v)) v = 0.0f;
            d[i] = v;
        }
    }

    // Safety: if processed audio is problematic, fall back to dry
    float minV = +1e9f, maxV = -1e9f;
    for (int ch = 0; ch < totalNumOutputChannels; ++ch)
    {
        auto* r = buffer.getReadPointer(ch);
        const float mn = juce::FloatVectorOperations::findMinimum (r, numSamples);
        const float mx = juce::FloatVectorOperations::findMaximum (r, numSamples);
        minV = std::min(minV, mn);
        maxV = std::max(maxV, mx);
    }
    const float wetPeak = std::max(std::abs(minV), std::abs(maxV));
    const bool wetDead = wetPeak < 1e-10f || !std::isfinite(wetPeak);

    if (wetDead)
    {
        for (int ch = 0; ch < totalNumOutputChannels; ++ch)
            buffer.copyFrom(ch, 0, tempDry, ch, 0, numSamples);
    }
    
    // 7. Real-time visualization data
    // Calculate RMS and peak values for meters
    {
        float rmsIn = 0.0f, rmsOut = 0.0f;
        float peakIn = 0.0f, peakOut = 0.0f;
        
        // Input levels (from dry buffer)
        for (int ch = 0; ch < totalNumOutputChannels; ++ch) {
            float chRms = tempDry.getRMSLevel(ch, 0, numSamples);
            float chPeak = tempDry.getMagnitude(ch, 0, numSamples);
            rmsIn = juce::jmax(rmsIn, chRms);
            peakIn = juce::jmax(peakIn, chPeak);
        }
        
        // Output levels (from processed buffer)
        for (int ch = 0; ch < totalNumOutputChannels; ++ch) {
            float chRms = buffer.getRMSLevel(ch, 0, numSamples);
            float chPeak = buffer.getMagnitude(ch, 0, numSamples);
            rmsOut = juce::jmax(rmsOut, chRms);
            peakOut = juce::jmax(peakOut, chPeak);
        }
        
        // Smooth the meters with fast attack, slower release
        const float attack = 0.3f;
        const float release = 0.05f;
        
        float currentRmsIn = vizRmsIn.load();
        vizRmsIn.store(rmsIn > currentRmsIn ? 
                      (attack * rmsIn + (1.0f - attack) * currentRmsIn) :
                      (release * rmsIn + (1.0f - release) * currentRmsIn));
        
        float currentRmsOut = vizRmsOut.load();
        vizRmsOut.store(rmsOut > currentRmsOut ? 
                       (attack * rmsOut + (1.0f - attack) * currentRmsOut) :
                       (release * rmsOut + (1.0f - release) * currentRmsOut));
        
        vizPeakIn.store(peakIn);
        vizPeakOut.store(peakOut);
    }
    
    // Push audio samples for spectrum analysis
    // Guard FIFO write sizes and pointers
    const float* L = buffer.getReadPointer(0);
    const float* R = (totalNumOutputChannels > 1) ? buffer.getReadPointer(1) : nullptr;
    if (L && numSamples > 0)
        pushVizSamples(L, R, numSamples);
    
    // 8. WAV Recording (post-processing)
    if (isRecording.load() && wavWriter) {
        recordBuffer.setSize(2, numSamples, false, false, true);
        for (int ch = 0; ch < juce::jmin(2, totalNumOutputChannels); ++ch) {
            recordBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);
        }
        wavWriter->writeFromAudioSampleBuffer(recordBuffer, 0, numSamples);
    }
}

//==============================================================================
bool FieldEngineAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* FieldEngineAudioProcessor::createEditor()
{
    return new FieldEngineAudioProcessorEditor (*this);
}

//==============================================================================
void FieldEngineAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Save plugin state
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void FieldEngineAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // Restore plugin state
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout FieldEngineAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Music Mode: the "anything = music" master switch
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "mode", "Routing",
        juce::StringArray{"Filter Only", "Chordifier Only", "Filter + Chordifier"}, 2)); // Default to Both

    // Z-plane morph targets
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "t1", "Morph T1", 0.0f, 1.0f, 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "t2", "Morph T2", 0.0f, 1.0f, 0.5f));

    // Filter parameters
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "cutoff", "Cutoff",
        juce::NormalisableRange<float>(20.0f, 20000.0f, 0.0f, 0.25f),
        1000.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "resonance", "Resonance", 0.0f, 1.0f, 0.0f));

    // Filter model
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        "model", "Filter Model", 1, 1201, 1012)); // Default to HyperQ-12

    // Chordifier parameters
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "key", "Key",
        juce::StringArray{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"}, 4)); // Default E

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "scale", "Scale",
        juce::StringArray{"Major","Minor"}, 0)); // Default Major

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "chord", "Chord Type",
        juce::StringArray{"MajorTriad","MinorTriad","Seventh","Power","Octaves"}, 0)); // Default MajorTriad

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "q", "Resonance Q",
        juce::NormalisableRange<float>(0.7f, 25.f, 0.f, 0.35f), 5.f)); // Default Q=5

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "drywet", "Dry/Wet", 0.0f, 1.0f, 0.8f)); // Default 80% wet

    // Z-Plane morphing parameters (new interface)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "drive", "Drive", 0.0f, 1.0f, 0.2f)); // Pre-drive gain

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "intensity", "Intensity", 0.0f, 1.0f, 0.4f)); // Resonance scaling

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "morph", "Morph", 0.0f, 1.0f, 0.0f)); // Shape A/B interpolation

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "autoMakeup", "Auto Makeup (RMS)", true)); // RMS-based gain compensation

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "sectSat", "Section Saturation", true)); // Per-section saturation enable

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "satAmt", "Saturation Amount", 0.0f, 1.0f, 0.2f)); // Saturation amount

    // Built-in Morph LFO (kept simple; editor controls optional)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "lfoRate", "LFO Rate (Hz)",
        juce::NormalisableRange<float>(0.02f, 8.0f, 0.0f, 0.35f), 0.20f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "lfoDepth", "LFO Depth",
        0.0f, 1.0f, 0.25f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "lfoStereo", "LFO Stereo Phase (deg)",
        0.0f, 180.0f, 90.0f));

    // Envelope → Morph (program-reactive motion)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "envDepth", "Env→Morph Depth", 0.0f, 1.0f, 0.35f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "envAttack", "Env Attack (ms)", juce::NormalisableRange<float>(0.5f, 80.0f, 0.0f, 0.35f), 5.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "envRelease", "Env Release (ms)", juce::NormalisableRange<float>(10.0f, 1000.0f, 0.0f, 0.35f), 120.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "envInvert", "Env Invert", false));

    // Blend parameter for parallel Filter/Chordifier mixing
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "blend", "Blend",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0f, 1.0f), 0.5f));

    // Theme customization parameters
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "themeHue", "Theme Hue", 0.0f, 1.0f, 0.52f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "themeSat", "Theme Saturation", 0.0f, 1.0f, 0.85f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "themeVal", "Theme Brightness", 0.2f, 1.0f, 0.95f));

    // New hub parameters for clean processing interface
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "hubBypass", "Bypass", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "hubSoloWet","Solo Wet", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "hubAudition","Audition", false));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "hubAuditionType","Audition Type", juce::StringArray{"Sine","Noise"}, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "hubAuditionLevel","Audition Level", 
        juce::NormalisableRange<float>(-36.f, 0.f, 0.f, 1.0f), -12.f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "midiLocal","Local MIDI", true));
    
    // Add serial routing parameter
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "serial", "Serial Routing", false)); // false = parallel (default)

    return { params.begin(), params.end() };
}

void FieldEngineAudioProcessor::updateHostTempo()
{
    // Get host tempo for tempo-sync LFOs (RT-safe)
    if (auto* playHead = getPlayHead())
    {
        juce::AudioPlayHead::CurrentPositionInfo positionInfo;
        if (playHead->getCurrentPosition(positionInfo) && positionInfo.bpm > 0.0)
        {
            double newBpm = positionInfo.bpm;
            if (std::abs(newBpm - lastBpm) > 0.1) // Avoid unnecessary updates
            {
                lastBpm = newBpm;
                dspEngine.setHostTempoBpm(newBpm);
            }
        }
    }
}

void FieldEngineAudioProcessor::loadShapePreset(const juce::String& presetId)
{
    // Load JSON preset files (Audity-style shapes)
    juce::File shapeAFile = juce::File::getCurrentWorkingDirectory().getChildFile("fieldEngine/audity_shapes_A_48k.json");
    juce::File shapeBFile = juce::File::getCurrentWorkingDirectory().getChildFile("fieldEngine/audity_shapes_B_48k.json");

    if (!shapeAFile.existsAsFile() || !shapeBFile.existsAsFile()) {
        return; // Skip if preset files missing
    }

    // Parse JSON files
    auto shapeAJson = juce::JSON::parse(shapeAFile);
    auto shapeBJson = juce::JSON::parse(shapeBFile);

    if (!shapeAJson.isObject() || !shapeBJson.isObject()) {
        return; // Skip if parsing failed
    }

    auto shapeAData = shapeAJson.getProperty(presetId, juce::var());
    auto shapeBData = shapeBJson.getProperty(presetId, juce::var());

    if (!shapeAData.isArray() || !shapeBData.isArray()) {
        return; // Skip if preset not found
    }

    // Parse shapes with current sample rate conversion
    auto newShapeA = parseShapeFromJson(shapeAData, static_cast<float>(currentSampleRate));
    auto newShapeB = parseShapeFromJson(shapeBData, static_cast<float>(currentSampleRate));

    // Thread-safe atomic shape update (write to inactive bank)
    int currentBank = audioShapeBankIndex.load();
    int newBank = 1 - currentBank; // flip to other bank

    // Update inactive bank
    audioShapeA_banks[newBank] = newShapeA;
    audioShapeB_banks[newBank] = newShapeB;

    // Atomic swap to new bank (audio thread will pick this up)
    audioShapeBankIndex.store(newBank);
}

float FieldEngineAudioProcessor::theta48_to_thetaFs(float theta48, float fs) const
{
    // Convert theta from 48kHz reference to target sample rate
    return theta48 * (48000.0f / fs);
}

std::array<PolePair, ZPLANE_N_SECTIONS> FieldEngineAudioProcessor::parseShapeFromJson(const juce::var& shapeVar, float fs) const
{
    std::array<PolePair, ZPLANE_N_SECTIONS> shape;

    // Initialize with safe default values
    for (int i = 0; i < ZPLANE_N_SECTIONS; ++i) {
        shape[i] = {0.5f, 0.0f}; // r=0.5, theta=0 (safe default)
    }

    if (!shapeVar.isArray()) return shape;

    auto shapeArray = *shapeVar.getArray();
    int sectionCount = std::min(static_cast<int>(shapeArray.size()), ZPLANE_N_SECTIONS);

    for (int i = 0; i < sectionCount; ++i) {
        auto section = shapeArray[i];
        if (section.isObject()) {
            float r = static_cast<float>(section.getProperty("r", 0.5));
            float theta48 = static_cast<float>(section.getProperty("theta", 0.0));

            // Convert theta from 48kHz to current sample rate
            float theta = theta48_to_thetaFs(theta48, fs);

            // Clamp for stability
            r = juce::jlimit(0.0f, 0.999999f, r);

            shape[i] = {r, theta};
        }
    }

    return shape;
}

void FieldEngineAudioProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    // Safety: Only sync parameters after DSP engine is initialized
    if (!engineInitialized.load()) {
        return;
    }

    // Sync APVTS parameters with DSP engine (thread-safe)
    if (parameterID == "t1" || parameterID == "t2")
    {
        float t1 = apvts.getRawParameterValue("t1")->load();
        float t2 = apvts.getRawParameterValue("t2")->load();
        dspEngine.setMorphTargets(t1, t2);
    }
    else if (parameterID == "cutoff" || parameterID == "resonance" || parameterID == "model")
    {
        int model = static_cast<int>(apvts.getRawParameterValue("model")->load());
        float cutoff = apvts.getRawParameterValue("cutoff")->load();
        float resonance = apvts.getRawParameterValue("resonance")->load();

        // Convert cutoff from Hz to 0-1 normalized
        float cutoffNormalized = juce::jmap(cutoff, 20.0f, 20000.0f, 0.0f, 1.0f);

        dspEngine.setFilter(model, cutoffNormalized, resonance);
    }
    else if (parameterID == "key" || parameterID == "scale" || parameterID == "chord" || parameterID == "q")
    {
        // Update Chordifier settings
        int key = static_cast<int>(apvts.getRawParameterValue("key")->load());
        int scale = static_cast<int>(apvts.getRawParameterValue("scale")->load());
        int chord = static_cast<int>(apvts.getRawParameterValue("chord")->load());
        float Q = apvts.getRawParameterValue("q")->load();

        // Convert key index to MIDI root note (E3 = MIDI 52 baseline)
        int rootMidi = fe::Chordifier::keyToMidi(key, 3);

        // Get chord intervals
        bool minor = (scale == 1);
        juce::String chordNames[] = {"MajorTriad", "MinorTriad", "Seventh", "Power", "Octaves"};
        juce::String chordType = chordNames[chord];

        auto intervals = fe::Chordifier::getChordIntervals(chordType.toStdString(), minor);

        // Apply to Chordifier
        chordifier.setChord(rootMidi, intervals, Q);
    }
    else if (parameterID == "drywet")
    {
        // GUI shows Wet%; Chordifier expects Dry mix
        const float wet = apvts.getRawParameterValue("drywet")->load();
        chordifier.dryMix_ = 1.0f - wet;
    }
    // Z-Plane morphing parameter handlers
    else if (parameterID == "drive")
    {
        dspEngine.setDrive(newValue);
    }
    else if (parameterID == "intensity")
    {
        dspEngine.setIntensity(newValue);
    }
    else if (parameterID == "morph")
    {
        // store as base; live LFO adds on top inside processBlock
        baseMorph = newValue;
    }
    else if (parameterID == "autoMakeup")
    {
        dspEngine.setAutoMakeup(newValue > 0.5f);
    }
    else if (parameterID == "sectSat")
    {
        dspEngine.enableSectionSaturation(newValue > 0.5f);
    }
    else if (parameterID == "satAmt")
    {
        dspEngine.setSectionSaturationAmount(newValue);
    }
    // ---- LFO controls ----
    else if (parameterID == "lfoRate")
    {
        lfoL.setRateHz(newValue);
        lfoR.setRateHz(newValue);
    }
    else if (parameterID == "lfoDepth")
    {
        lfoDepth = newValue;
    }
    else if (parameterID == "lfoStereo")
    {
        lfoR.setPhaseOffset(newValue);
    }
    // ---- Env→Morph controls ----
    else if (parameterID == "envDepth")
    {
        envMod.depth = newValue;
    }
    else if (parameterID == "envAttack" || parameterID == "envRelease")
    {
        const float atk = apvts.getRawParameterValue("envAttack")->load();
        const float rel = apvts.getRawParameterValue("envRelease")->load();
        envMod.prepare(currentSampleRate, atk, rel);
    }
    else if (parameterID == "envInvert")
    {
        envMod.invert = (newValue > 0.5f);
    }
    else if (parameterID == "blend")
    {
        // Blend parameter handled in processBlock for parallel mixing
        // No audio thread work needed here
    }
    else if (parameterID == "themeHue" || parameterID == "themeSat" || parameterID == "themeVal")
    {
        // Theme parameters polled by editor timer for live updates
        // No audio thread work needed here
    }
}

void FieldEngineAudioProcessor::loadDefaultShapes()
{
    // Create sensible default Z-plane shapes for testing
    std::array<PolePair, ZPLANE_N_SECTIONS> defaultShapeA;
    std::array<PolePair, ZPLANE_N_SECTIONS> defaultShapeB;

    // Shape A: Low-pass character (safe, musical defaults)
    for (int i = 0; i < ZPLANE_N_SECTIONS; ++i) {
        defaultShapeA[i].r = 0.92f - (i * 0.02f);  // Decreasing radius
        defaultShapeA[i].theta = 0.1f + (i * 0.05f); // Low frequencies
    }

    // Shape B: High-pass character for morphing contrast
    for (int i = 0; i < ZPLANE_N_SECTIONS; ++i) {
        defaultShapeB[i].r = 0.94f - (i * 0.015f);
        defaultShapeB[i].theta = 0.3f + (i * 0.1f);  // Higher frequencies
    }

    // Load into both banks
    audioShapeA_banks[0] = defaultShapeA;
    audioShapeA_banks[1] = defaultShapeA;
    audioShapeB_banks[0] = defaultShapeB;
    audioShapeB_banks[1] = defaultShapeB;

    // Immediately apply to DSP engine
    dspEngine.setShapeA(defaultShapeA);
    dspEngine.setShapeB(defaultShapeB);
}

void FieldEngineAudioProcessor::forceParameterSync()
{
    // Force all parameters to sync with DSP engine by calling parameterChanged
    // This ensures initialization happens even if listeners weren't triggered

    const float cutoff = apvts.getRawParameterValue("cutoff")->load();
    const float resonance = apvts.getRawParameterValue("resonance")->load();
    const float drive = apvts.getRawParameterValue("drive")->load();
    const float intensity = apvts.getRawParameterValue("intensity")->load();
    const float morph = apvts.getRawParameterValue("morph")->load();
    const float drywet = apvts.getRawParameterValue("drywet")->load();

    // Apply core DSP parameters
    dspEngine.setDrive(drive);
    dspEngine.setIntensity(intensity);
    dspEngine.setMorph(morph);

    // Apply Chordifier dry/wet (GUI shows wet%, Chordifier expects dry mix)
    chordifier.dryMix_ = 1.0f - drywet;

    // Apply boolean parameters
    const bool autoMakeup = apvts.getRawParameterValue("autoMakeup")->load() > 0.5f;
    const bool sectSat = apvts.getRawParameterValue("sectSat")->load() > 0.5f;
    const float satAmt = apvts.getRawParameterValue("satAmt")->load();

    dspEngine.setAutoMakeup(autoMakeup);
    dspEngine.enableSectionSaturation(sectSat);
    dspEngine.setSectionSaturationAmount(satAmt);

    // Store morph as base for LFO modulation
    baseMorph = morph;
}

//==============================================================================
// WAV Recording Implementation
void FieldEngineAudioProcessor::startRecording()
{
    if (isRecording.load())
        return;
    
    // Create output file with timestamp
    juce::Time now = juce::Time::getCurrentTime();
    juce::String filename = juce::String("fieldEngine-render-") + 
                           now.formatted("%Y%m%d-%H%M%S") + ".wav";
    
    juce::File documentsDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    wavOutputFile = documentsDir.getChildFile(filename);
    
    // Create WAV writer
    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::FileOutputStream> outputStream(wavOutputFile.createOutputStream());
    
    if (outputStream != nullptr)
    {
        wavWriter.reset(wavFormat.createWriterFor(outputStream.release(),
                                                 currentSampleRate,
                                                 2, // stereo
                                                 24, // 24-bit
                                                 {}, 0));
        if (wavWriter != nullptr)
            isRecording.store(true);
    }
}

void FieldEngineAudioProcessor::stopRecording()
{
    if (!isRecording.load())
        return;
    
    isRecording.store(false);
    wavWriter.reset(); // This flushes and closes the file
    
    // Reveal file to user
    if (wavOutputFile.exists())
        wavOutputFile.revealToUser();
}

//==============================================================================
// MIDI from UI
void FieldEngineAudioProcessor::pushMidiNote(bool isNoteOn, int noteNumber, float velocity)
{
    const auto scope = midiEventFifo.write(1);
    if (scope.blockSize1 > 0)
    {
        midiEventBuffer[scope.startIndex1] = { isNoteOn, noteNumber, velocity };
    }
}

//==============================================================================
// Visualization data pipeline
void FieldEngineAudioProcessor::pushVizSamples(const float* L, const float* R, int numSamples)
{
    // Create mono mix for visualization using pre-allocated buffer
    static thread_local std::vector<float> monoBuffer;
    // Ensure capacity before writing
    if ((int) monoBuffer.size() < numSamples)
        monoBuffer.resize((size_t) numSamples, 0.0f);

    if (R) {
        // Stereo: average L+R
        for (int i = 0; i < numSamples; ++i) {
            monoBuffer[i] = 0.5f * (L[i] + R[i]);
        }
    } else {
        // Mono: just copy
        std::copy(L, L + numSamples, monoBuffer.begin());
    }
    
    // Push to ring buffer
    int start1, size1, start2, size2;
    vizFifo.prepareToWrite(numSamples, start1, size1, start2, size2);
    
    if (size1 > 0) {
        vizBuffer.copyFrom(0, start1, monoBuffer.data(), size1);
    }
    if (size2 > 0) {
        vizBuffer.copyFrom(0, start2, monoBuffer.data() + size1, size2);
    }
    
    vizFifo.finishedWrite(size1 + size2);
}

//==============================================================================
// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FieldEngineAudioProcessor();
}