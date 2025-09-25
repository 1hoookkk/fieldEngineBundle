#include "FieldEngineFXProcessor.h"
#include "FieldEngineFXEditor.h"
#include "../ui/ViralEditor.h"
#include <map>
#include "../shared/ZPlaneTables.h"
#include <cmath>
#include <algorithm>

namespace { static inline float clamp01(float v){ return juce::jlimit(0.0f, 1.0f, v); } }

FieldEngineFXProcessor::FieldEngineFXProcessor()
: juce::AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
, parameters (*this, nullptr, juce::Identifier("FieldEngineFX"), createParameterLayout())
{
    // RT-SAFE: Load authentic pole data in constructor (background thread)
    loadEMUShapeDataAsync();
}

void FieldEngineFXProcessor::loadEMUShapeDataAsync()
{
    // Use background thread for file I/O to avoid RT-thread blocking
    juce::Thread::launch([this]()
    {
        auto loadShapeMap = [](const juce::File& file) -> std::map<juce::String, std::array<fe::PolePair, fe::ZPLANE_N_SECTIONS>>
        {
            std::map<juce::String, std::array<fe::PolePair, fe::ZPLANE_N_SECTIONS>> out;
            if (!file.existsAsFile()) return out;
            
            try {
                auto parsed = juce::JSON::parse(file.loadFileAsString());
                if (!parsed.isObject()) return out;
                auto shapesVar = parsed.getProperty("shapes", juce::var());
                if (!shapesVar.isArray()) return out;
                auto* shapesArr = shapesVar.getArray();
                
                for (auto& sv : *shapesArr)
                {
                    if (!sv.isObject()) continue;
                    auto id = sv.getProperty("id", juce::var()).toString();
                    auto polesVar = sv.getProperty("poles", juce::var());
                    if (!polesVar.isArray()) continue;
                    
                    std::array<fe::PolePair, fe::ZPLANE_N_SECTIONS> poles{};
                    auto* pa = polesVar.getArray();
                    const int n = std::min<int>((int)pa->size(), fe::ZPLANE_N_SECTIONS);
                    
                    for (int i = 0; i < n; ++i)
                    {
                        auto pv = (*pa)[i];
                        if (!pv.isObject()) continue;
                        float r = (float) pv.getProperty("r", 0.95f);
                        float theta = (float) pv.getProperty("theta", 0.0f);
                        poles[(size_t)i] = fe::PolePair{ juce::jlimit(0.0f, 0.999999f, r), theta };
                    }
                    out[id] = poles;
                }
            }
            catch (...)
            {
                // Silently fail and return empty map - use built-in defaults
            }
            return out;
        };

        auto findCandidates = []() -> std::pair<juce::File, juce::File>
        {
            // Cross-platform relative paths - no hardcoded Windows paths
            juce::File appDir = juce::File::getSpecialLocation(juce::File::currentApplicationFile).getParentDirectory();
            
            // Try new inventory structure first
            juce::File cwd = juce::File::getCurrentWorkingDirectory();
            juce::File a1 = cwd.getChildFile("inventory/shapes/audity_shapes_A_48k.json");
            juce::File b1 = cwd.getChildFile("inventory/shapes/audity_shapes_B_48k.json");
            if (a1.existsAsFile() && b1.existsAsFile()) return { a1, b1 };

            // Fallback to plugin relative paths
            juce::File a2 = appDir.getChildFile("../../../inventory/shapes/audity_shapes_A_48k.json");
            juce::File b2 = appDir.getChildFile("../../../inventory/shapes/audity_shapes_B_48k.json");
            if (a2.existsAsFile() && b2.existsAsFile()) return { a2, b2 };
            
            return { juce::File(), juce::File() };
        };

        auto [fileA, fileB] = findCandidates();
        if (fileA.existsAsFile() && fileB.existsAsFile())
        {
            auto tempShapeMapA = loadShapeMap(fileA);
            auto tempShapeMapB = loadShapeMap(fileB);
            
            // Atomically update the shape maps (RT-safe)
            shapeMapA48k = std::move(tempShapeMapA);
            shapeMapB48k = std::move(tempShapeMapB);
            
            // Set default pair using atomic flag
            shapesLoaded.store(true);
        }
        // else: keep built-in defaults
    });
}

FieldEngineFXProcessor::~FieldEngineFXProcessor() = default;

float FieldEngineFXProcessor::generateLFOSample(LFOShape shape, float phase)
{
    // Ensure phase is in [0, 1] range
    phase = std::fmod(phase, 1.0f);
    if (phase < 0.0f) phase += 1.0f;

    switch (shape)
    {
        case Sine:
            return std::sin(phase * juce::MathConstants<float>::twoPi);

        case Triangle:
            return (phase < 0.5f) ?
                   (4.0f * phase - 1.0f) :     // Rising edge: -1 to 1
                   (3.0f - 4.0f * phase);       // Falling edge: 1 to -1

        case Square:
            return (phase < 0.5f) ? 1.0f : -1.0f;

        case Saw:
            return 2.0f * phase - 1.0f;  // Ramp from -1 to 1

        default:
            return std::sin(phase * juce::MathConstants<float>::twoPi);
    }
}

void FieldEngineFXProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);
    
    // RT-SAFE: Validate sample rate to prevent audio artifacts
    if (sampleRate < 8000.0 || sampleRate > 192000.0)
    {
        juce::Logger::writeToLog("FieldEngineFX: Invalid sample rate " + juce::String(sampleRate) + "Hz, using 44100Hz");
        sampleRate = 44100.0;
    }
    
    currentSampleRate.store(sampleRate);
    morphFilter.prepare(sampleRate, samplesPerBlock);
    morphFilter.reset(); // Reset filter state
    
    for (auto& f : channelFilters) {
        f.prepareToPlay(sampleRate);
        // Using AuthenticEMUZPlane now, no setFilterType needed
        f.reset(); // Reset filter state
    }
    
    // Initialize EMU filter models
    for (auto& filterModel : emuFilterModels) {
        filterModel.prepareToPlay(sampleRate);
        filterModel.reset(); // Reset filter state
    }

    // Prepare Z-plane morphing filter engine
    zFilter.prepare(sampleRate, samplesPerBlock);
    zFilter.enableSectionSaturation(true);
    zFilter.setSectionSaturationAmount(0.25f);
    zFilter.setAutoMakeup(true);

    // Prepare neutral morph engine for UI telemetry
    morphEngine.prepare(sampleRate, samplesPerBlock, getTotalNumInputChannels());
    telemetryRing.assign((size_t)telemetryFifo.getTotalSize(), 0.0f);

    // AUTHENTIC morphing with real extracted coefficients
    authenticEMU.prepareToPlay(sampleRate);
    // Set viral defaults for immediate appeal
    authenticEMU.setMorphPair(AuthenticEMUZPlane::VowelAe_to_VowelOo);
    authenticEMU.setMorphPosition(0.5f);
    authenticEMU.setIntensity(0.6f);        // High intensity for character
    authenticEMU.setDrive(6.0f);           // 6dB drive for warmth
    authenticEMU.setLFORate(1.2f);         // Fast LFO movement
    authenticEMU.setLFODepth(0.25f);       // Noticeable modulation

    // Initialize smoothers
    morphSmoother.reset(sampleRate, 0.05);
    intensitySmoother.reset(sampleRate, 0.05);
    driveSmoother.reset(sampleRate, 0.05);
    outputSmoother.reset(sampleRate, 0.05);
    mixSmoother.reset(sampleRate, 0.05);
    lfoRateSmoother.reset(sampleRate, 0.05);
    lfoAmountSmoother.reset(sampleRate, 0.05);

    // RT-SAFE: File I/O moved to constructor - no file operations in prepareToPlay
    
    lfo.setSampleRate((float) sampleRate);
    lfo.setFrequency(1.0f);
    lfo.setTargetAmplitude(1.0f);

    // Initialize enhanced envelope follower
    envelopeFollower.setSampleRate(sampleRate);
    envelopeFollower.reset();
    // sample rate already stored at start
    
    // Reset all band energies
    for (auto& energy : bandEnergy) {
        energy.store(0.0f);
    }
}

void FieldEngineFXProcessor::releaseResources() {}

bool FieldEngineFXProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    auto in = layouts.getMainInputChannelSet();
    auto out = layouts.getMainOutputChannelSet();
    if (in == juce::AudioChannelSet::disabled() || out == juce::AudioChannelSet::disabled()) return false;
    if (in != out) return false;
    if (in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo()) return false;
    return true;
}

void FieldEngineFXProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused(midi);
    juce::ScopedNoDenormals _;

    auto getParam = [this](const juce::String& id, float defVal){ if (auto* p = parameters.getRawParameterValue(id)) return p->load(); return defVal; };

    // Update smoothers with target values from parameters
    morphSmoother.setTargetValue(getParam("MORPH", 0.5f));
    intensitySmoother.setTargetValue(getParam("intensity", 0.6f)); // Z-plane intensity parameter
    driveSmoother.setTargetValue(getParam("DRIVE", 0.5f));
    outputSmoother.setTargetValue(getParam("OUTPUT", 1.0f));
    mixSmoother.setTargetValue(getParam("mix", 1.0f));

    // Movement Rate with BPM sync (like EMU hardware)
    float movementRate = getParam("movementRate", 1.2f);
    int sync = (int) getParam("sync", 0.0f); // 0=Free

    // FabFilter-style solo modes: 0=Off, 1=Wet, 2=Dry, 3=Diff
    int soloMode = (int) getParam("solo", 0.0f);
    bool isBypassed = getParam("BYPASS", 0.0f) > 0.5f;

    // BPM sync for movement rate (like EMU hardware)
    if (sync > 0) {
        double hostBPM = 120.0; // Default fallback BPM
        if (auto* playhead = getPlayHead()) {
            auto posInfo = playhead->getPosition();
            if (posInfo.hasValue() && posInfo->getBpm().hasValue()) {
                hostBPM = *posInfo->getBpm();
            }
        }

        // Convert sync index to note division:
        // sync: 1 -> 1/4, 2 -> 1/8, 3 -> 1/16, 4 -> 1/32
        float division = 1.0f / (1 << (sync + 1)); // 1/4, 1/8, 1/16, 1/32
        float syncRate = static_cast<float>(hostBPM / 60.0) / division; // Hz
        lfo.setFrequency(juce::jlimit(0.01f, 50.0f, syncRate));
    } else {
        lfo.setFrequency(juce::jlimit(0.01f, 20.0f, movementRate));
    }

    const int numSamples = buffer.getNumSamples();
    const int channels = buffer.getNumChannels();

    // Pair switching: 0=vowel, 1=bell, 2=low
    int pairIdx = 0;
    if (auto* p = parameters.getRawParameterValue("pair")) pairIdx = (int)p->load();
    if (pairIdx != lastPairIndex) {
        if (applyPairByIndex(pairIdx))
            lastPairIndex = pairIdx;
    }

    // Calculate modulated morph value once per block
    float lfoValue = lfo.generateSample();
    float morph = clamp01(morphSmoother.getNextValue() + lfoValue * lfoAmountSmoother.getNextValue() * 0.5f);
    float uiMorph = morph; // snapshot for UI
    
    // Use Z-Plane tables for authentic EMU frequency and resonance mapping
    float freq = ZPlaneTables::T1_TABLE_lookup(morph);
    float q = ZPlaneTables::T2_TABLE_lookup(morph);
    
    // Safety limits on frequency and resonance
    freq = juce::jlimit(20.0f, 20000.0f, freq);
    q = juce::jlimit(0.1f, 15.0f, q); // Keep full Q scale for Z-plane intensity mapping
    
    // Get EMU filter model selection (retained for UI; not used by zFilter)
    int filterModelIndex = parameters.getRawParameterValue("filterModel")->load();
    juce::ignoreUnused(filterModelIndex);

    // Add safety check and gentle processing
    float intensity = intensitySmoother.getNextValue();
    float drive = driveSmoother.getNextValue();

    // Update neutral morph engine params for telemetry mapping
    fe::MorphParams mp;
    mp.driveDb = juce::jmap(drive, 0.1f, 2.0f, -12.0f, 18.0f);
    mp.focus01 = morph;
    mp.contour = juce::jmap(intensity, 0.0f, 1.0f, -1.0f, 1.0f);
    morphEngine.setParams(mp);
    
    // Only process if intensity is above threshold to avoid artifacts
    if (!isBypassed && intensity > 0.01f) {
        // Preserve dry if mixing is needed
        juce::AudioBuffer<float> dry;
        float mix = mixSmoother.getNextValue();
        if (soloMode == 0 && mix < 0.999f)  // Only preserve dry in normal mix mode
            dry.makeCopyOf(buffer);
            
        // Safety limits on all parameters
        drive = juce::jlimit(0.1f, 8.0f, drive);  // Full drive range
        intensity = juce::jlimit(0.0f, 1.0f, intensity);  // Full intensity
        morph = juce::jlimit(0.0f, 1.0f, morph);
        
        // Route parameters to Z-plane engine with full scaling
        zFilter.setDrive(juce::jlimit(0.0f, 1.0f, (drive - 0.1f) / (8.0f - 0.1f)));  // Full drive range
        zFilter.setIntensity(intensity);
        zFilter.setMorph(morph);
        zFilter.updateCoefficientsBlock();

        float* left = buffer.getWritePointer(0);
        float* right = (channels > 1) ? buffer.getWritePointer(1) : buffer.getWritePointer(0);
        
        // RT-SAFE: Apply proper gain staging and NaN protection
        buffer.applyGain(1.0f);  // Full input level
        
        // RT-SAFE: Protect against NaN/infinity before processing
        for (int ch = 0; ch < channels; ++ch)
        {
            float* channelData = buffer.getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i)
            {
                if (!std::isfinite(channelData[i]))
                    channelData[i] = 0.0f;
            }
        }
        
        zFilter.processBlock(left, right, numSamples);

        // Tap mono mix for visual telemetry (lock-free)
        {
            int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
            auto toPush = std::min(numSamples, telemetryFifo.getFreeSpace());
            telemetryFifo.prepareToWrite(toPush, start1, size1, start2, size2);
            if (size1 > 0) {
                for (int i = 0; i < size1; ++i) telemetryRing[(size_t)(start1 + i)] = 0.5f * (left[i] + right[i]);
            }
            if (size2 > 0) {
                for (int i = 0; i < size2; ++i) telemetryRing[(size_t)(start2 + i)] = 0.5f * (left[size1 + i] + right[size1 + i]);
            }
            telemetryFifo.finishedWrite(size1 + size2);
        }
        
        // RT-SAFE: Protect against NaN/infinity after processing
        for (int ch = 0; ch < channels; ++ch)
        {
            float* channelData = buffer.getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i)
            {
                if (!std::isfinite(channelData[i]))
                    channelData[i] = 0.0f;
                channelData[i] = juce::jlimit(-2.0f, 2.0f, channelData[i]); // Hard limit
            }
        }

        // FabFilter-style solo modes
        switch (soloMode) {
            case 0: // Off - Normal dry/wet mix
                if (mix < 0.999f) {
                    mix = juce::jlimit(0.0f, 1.0f, mix);
                    for (int ch = 0; ch < channels; ++ch) {
                        buffer.applyGain(ch, 0, numSamples, mix);
                        buffer.addFrom(ch, 0, dry.getReadPointer(ch), numSamples, 1.0f - mix);
                    }
                }
                break;

            case 1: // Wet - Filter output only
                // Buffer already contains filtered signal, do nothing
                break;

            case 2: // Dry - Unprocessed signal only
                buffer.makeCopyOf(dry);
                break;

            case 3: // Diff - Difference between wet and dry
                for (int ch = 0; ch < channels; ++ch) {
                    for (int i = 0; i < numSamples; ++i) {
                        float wetSample = buffer.getSample(ch, i);
                        float drySample = dry.getSample(ch, i);
                        buffer.setSample(ch, i, wetSample - drySample);
                    }
                }
                break;
        }
    }


    buffer.applyGain(outputSmoother.getNextValue());
    
    // Apply safety limiting to prevent loud sounds
    for (int ch = 0; ch < channels; ++ch) {
        auto* data = buffer.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i) {
            data[i] = juce::jlimit(-1.0f, 1.0f, data[i]);
        }
    }

    // Snapshot morph telemetry for UI
    {
        auto t = morphEngine.getAndResetTelemetry();
        teleRmsL.store(t.rmsL); teleRmsR.store(t.rmsR);
        telePeakL.store(t.peakL); telePeakR.store(t.peakR);
        teleMorphX.store(t.morphX); teleMorphY.store(t.morphY);
        teleClipped.store(t.clipped ? 1 : 0);
    }

    // Process envelope follower
    auto* left = buffer.getReadPointer(0);
    for (int s = 0; s < numSamples; ++s) {
        envelopeFollower.process(left[s]);
    }

    // Update envelope follower with enhanced controls
    if (auto* envAttackParam = parameters.getRawParameterValue("envAttack"))
        envelopeFollower.setAttack(envAttackParam->load());
    if (auto* envReleaseParam = parameters.getRawParameterValue("envRelease"))
        envelopeFollower.setRelease(envReleaseParam->load());

    // Process envelope follower
    float envValue = 0.0f;
    for (int s = 0; s < numSamples; ++s) {
        envValue = envelopeFollower.process(buffer.getSample(0, s));
    }

    // Apply envelope depth to modulation
    float envDepth = getParam("envDepth", 0.945f);
    float envMod = envValue * envDepth;

    // Get LFO parameters
    float lfoDepth = getParam("lfoDepth", 0.758f);
    int lfoShapeIndex = (int)getParam("lfoShape", 0.0f);
    currentLFOShape = static_cast<LFOShape>(lfoShapeIndex);

    // Update UI state atomics once per block (lock-free)
    masterAlpha.store(uiMorph + envMod * 0.2f);  // Add envelope modulation to UI
    // Simple visuals for bands (not sonic)
    for (int i = 0; i < kNumBands; ++i)
    {
        float e = bandEnergy[i].load();
        float target = 0.15f + 0.75f * (0.5f * (1.0f + std::sin(0.1f * (float)i + 0.03f * uiMorph)));
        e = 0.9f * e + 0.1f * target;
        bandEnergy[i].store(juce::jlimit(0.0f, 1.0f, e));

        float a2 = bandAlpha[i].load();
        a2 = 0.95f * a2 + 0.05f * uiMorph;
        bandAlpha[i].store(juce::jlimit(0.0f, 1.0f, a2));

        float g = bandGainDb[i].load();
        g = 0.98f * g + 0.02f * juce::jmap(uiMorph, -6.0f, 6.0f);
        bandGainDb[i].store(g);
    }
}

juce::AudioProcessorEditor* FieldEngineFXProcessor::createEditor()
{
    // Use viral high-contrast UI with reactive visuals
    return new ViralEditor(*this);
}

bool FieldEngineFXProcessor::applyPairByIndex(int index)
{
    const juce::String ids[3] = { "vowel_pair", "bell_pair", "low_pair" };
    if (index < 0 || index > 2) return false;
    auto id = ids[index];
    auto itA = shapeMapA48k.find(id);
    auto itB = shapeMapB48k.find(id);
    if (itA == shapeMapA48k.end() || itB == shapeMapB48k.end()) return false;
    // Scale authentic 48k poles to current fs
    const double fsRef = 48000.0;
    const double fsCur = currentSampleRate.load();
    const float ratio = (float)(fsRef / (fsCur > 1.0 ? fsCur : fsRef));
    auto wrapPi = [](float a){ const float pi = juce::MathConstants<float>::pi; const float twoPi = juce::MathConstants<float>::twoPi; float x = std::fmod(a + pi, twoPi); if (x < 0) x += twoPi; return x - pi; };

    std::array<fe::PolePair, fe::ZPLANE_N_SECTIONS> scaledA{};
    std::array<fe::PolePair, fe::ZPLANE_N_SECTIONS> scaledB{};
    for (int i = 0; i < fe::ZPLANE_N_SECTIONS; ++i)
    {
        auto pA = itA->second[(size_t)i];
        auto pB = itB->second[(size_t)i];
        float rA = std::pow(std::clamp(pA.r, 0.0f, 0.999999f), ratio);
        float rB = std::pow(std::clamp(pB.r, 0.0f, 0.999999f), ratio);
        float tA = wrapPi(pA.theta * ratio);
        float tB = wrapPi(pB.theta * ratio);
        scaledA[(size_t)i] = { juce::jlimit(0.0f, 0.999999f, rA), tA };
        scaledB[(size_t)i] = { juce::jlimit(0.0f, 0.999999f, rB), tB };
    }
    zFilter.setShapeA(scaledA);
    zFilter.setShapeB(scaledB);
    return true;
}

void FieldEngineFXProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    if (state.isValid())
        if (auto xml = state.createXml())
            copyXmlToBinary(*xml, destData);
}

void FieldEngineFXProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorValueTreeState::ParameterLayout FieldEngineFXProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    auto add = [&params](auto p){ params.push_back(std::move(p)); };

    // ONLY Z-PLANE MORPHING PARAMETERS - EMU AUTHENTIC DEFAULTS
    add (std::make_unique<juce::AudioParameterFloat>("MORPH", "Morph", juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
    add (std::make_unique<juce::AudioParameterFloat>("intensity", "Intensity", juce::NormalisableRange<float>(0.0f, 1.0f), 0.758f));  // EMU Planet Phatt modulation depth
    add (std::make_unique<juce::AudioParameterFloat>("DRIVE", "Drive", juce::NormalisableRange<float>(0.1f, 2.0f), 0.8f));
    add (std::make_unique<juce::AudioParameterFloat>("mix", "Mix", juce::NormalisableRange<float>(0.0f, 1.0f), 1.0f));

    // Morph pairs (vowel/bell/low)
    juce::StringArray pairChoices; pairChoices.add("vowel"); pairChoices.add("bell"); pairChoices.add("low");
    add (std::make_unique<juce::AudioParameterChoice>("pair", "Pair", pairChoices, 0));

    // FABFILTER-STYLE SOLOING
    juce::StringArray soloChoices;
    soloChoices.add("Off");           // Normal dry/wet mix
    soloChoices.add("Wet");           // Filter output only
    soloChoices.add("Dry");           // Unprocessed signal only
    soloChoices.add("Diff");          // Difference (wet - dry)
    add (std::make_unique<juce::AudioParameterChoice>("solo", "Solo", soloChoices, 0));

    // BPM sync for movement rate
    add (std::make_unique<juce::AudioParameterFloat>("movementRate", "Movement Rate", juce::NormalisableRange<float>(0.01f, 20.0f, 0.0f, 0.5f), 0.05f));
    juce::StringArray syncChoices; syncChoices.add("Free"); syncChoices.add("1/4"); syncChoices.add("1/8"); syncChoices.add("1/16"); syncChoices.add("1/32");
    add (std::make_unique<juce::AudioParameterChoice>("sync", "Sync", syncChoices, 0));

    // AUTHENTIC X3 MODULATION
    add (std::make_unique<juce::AudioParameterFloat>("lfoDepth", "LFO Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.758f));

    add (std::make_unique<juce::AudioParameterFloat>("envAttack", "Env Attack",
        juce::NormalisableRange<float>(0.0001f, 2.0f, 0.0f, 0.3f), 0.000489f));

    add (std::make_unique<juce::AudioParameterFloat>("envRelease", "Env Release",
        juce::NormalisableRange<float>(0.001f, 5.0f, 0.0f, 0.3f), 0.08f));

    add (std::make_unique<juce::AudioParameterFloat>("envDepth", "Env Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.945f));

    add (std::make_unique<juce::AudioParameterFloat>("keyTracking", "Key Tracking",
        juce::NormalisableRange<float>(0.0f, 2.0f), 1.0f));

    add (std::make_unique<juce::AudioParameterFloat>("velocitySens", "Velocity Sens",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));

    // LFO waveform selection
    juce::StringArray lfoShapes;
    lfoShapes.add("Sine"); lfoShapes.add("Triangle"); lfoShapes.add("Square"); lfoShapes.add("Saw");
    add (std::make_unique<juce::AudioParameterChoice>("lfoShape", "LFO Shape", lfoShapes, 0));

    add (std::make_unique<juce::AudioParameterBool>("BYPASS", "Bypass", false));

    return { params.begin(), params.end() };
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FieldEngineFXProcessor();
}

void FieldEngineFXProcessor::drainTelemetry(float* dst, int maxToRead, int& numRead)
{
    numRead = 0;
    if (maxToRead <= 0 || dst == nullptr) return;
    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    auto toRead = std::min(maxToRead, telemetryFifo.getNumReady());
    telemetryFifo.prepareToRead(toRead, start1, size1, start2, size2);
    if (size1 > 0) {
        std::memcpy(dst, telemetryRing.data() + (size_t)start1, (size_t)size1 * sizeof(float));
        numRead += size1;
    }
    if (size2 > 0) {
        std::memcpy(dst + numRead, telemetryRing.data() + (size_t)start2, (size_t)size2 * sizeof(float));
        numRead += size2;
    }
    telemetryFifo.finishedRead(numRead);
}

void FieldEngineFXProcessor::getMorphTelemetry(float& rmsL, float& rmsR, float& peakL, float& peakR, float& morphX, float& morphY, bool& clipped) const
{
    rmsL = teleRmsL.load();
    rmsR = teleRmsR.load();
    peakL = telePeakL.load();
    peakR = telePeakR.load();
    morphX = teleMorphX.load();
    morphY = teleMorphY.load();
    clipped = teleClipped.load() != 0;
}
