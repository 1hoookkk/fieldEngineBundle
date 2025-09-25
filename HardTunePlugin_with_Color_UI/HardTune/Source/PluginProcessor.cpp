
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

enum ParamID {
    pidKey, pidScale, pidRetune, pidAmount, pidFormant, pidThroat, pidMix, pidMode, pidBias, pidInputType, pidColor
};

static const char* MODE_NAMES[] = { "Track", "Print" };

HardTuneAudioProcessor::HardTuneAudioProcessor()
: juce::AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
  apvts (*this, nullptr, "PARAMS", createParameterLayout())
{}

void HardTuneAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    maxBlock = samplesPerBlock;

    tracker.prepare(sr, samplesPerBlock);
    shifter.prepare(sr, Shifter::Mode::TrackPSOLA);
    ratioBuf.assign((size_t)samplesPerBlock, 1.0f);
    tmpL.assign((size_t)samplesPerBlock, 0.0f);
    tmpR.assign((size_t)samplesPerBlock, 0.0f);
    wetBuf.setSize(2, samplesPerBlock);

    emu.prepareToPlay(sr);
    emu.setMorphPair(0);        // default shape pair
    emu.setMorphPosition(0.5f); // center
    emu.setAutoMakeup(true);

    updateSnapper();
    updateRanges();
    updateColor();
}

bool HardTuneAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    auto mainIn  = layouts.getMainInputChannelSet();
    auto mainOut = layouts.getMainOutputChannelSet();
    return mainIn == mainOut && (mainIn == juce::AudioChannelSet::mono() || mainIn == juce::AudioChannelSet::stereo());
}

void HardTuneAudioProcessor::updateSnapper()
{
    const int key   = (int)apvts.getRawParameterValue("key")->load();
    const int scale = (int)apvts.getRawParameterValue("scale")->load();
    snapper.setKey(key, scale);
}

void HardTuneAudioProcessor::updateRanges()
{
    const int inputType = (int)apvts.getRawParameterValue("inputType")->load();
    float fmin=80.f, fmax=800.f;
    switch (inputType) {
        case 0: fmin=165.f; fmax=1100.f; break; // Soprano
        case 1: fmin=130.f; fmax=880.f;  break; // Alto
        case 2: fmin=98.f;  fmax=660.f;  break; // Tenor
        case 3: fmin=82.f;  fmax=520.f;  break; // Baritone
        case 4: fmin=65.f;  fmax=392.f;  break; // Bass
        default: break;
    }
    tracker.setRange(fmin, fmax);
}

void HardTuneAudioProcessor::updateColor()
{
    const float c = apvts.getRawParameterValue("color")->load(); // 0..1
    emu.setIntensity(c);                               // spectral emphasis
    emu.setDrive(juce::jmap(c, 0.0f, 1.0f, 0.0f, 3.0f));         // 0..+3 dB
    emu.setSectionSaturation(juce::jmin(0.35f, 0.25f * c));      // mild nonlinearity
    emu.setLFODepth(0.0f);                             // no auto-morph by default
    emu.setMorphPosition(0.5f);                        // neutral morph center
}

void HardTuneAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int numChans   = buffer.getNumChannels();

    // 1) Update DSP config if changed
    const float retune01 = apvts.getRawParameterValue("retune")->load();
    const float amount   = apvts.getRawParameterValue("amount")->load();
    const int   bias     = (int)apvts.getRawParameterValue("bias")->load();
    const int   mode     = (int)apvts.getRawParameterValue("mode")->load();
    const float mix      = apvts.getRawParameterValue("mix")->load();

    const float formant  = apvts.getRawParameterValue("formant")->load(); // semitones
    const float throat   = apvts.getRawParameterValue("throat")->load();
    (void)formant; (void)throat; // placeholder
    updateColor();

    // Change shifter mode based on quality
    shifter.prepare(sr, mode == 0 ? Shifter::Mode::TrackPSOLA : Shifter::Mode::PrintHQ);

    // 2) Mono detect: use L if mono, else average L+R into mono
    const float* inL = buffer.getReadPointer(0);
    std::vector<float> mono((size_t)numSamples);
    if (numChans == 1) {
        std::copy(inL, inL + numSamples, mono.begin());
    } else {
        const float* inR = buffer.getReadPointer(1);
        for (int i=0;i<numSamples;++i) mono[(size_t)i] = 0.5f * (inL[i] + inR[i]);
    }

    // 3) Track pitch and build ratio curve
    tracker.setRetune(retune01, bias);
    tracker.process(mono.data(), numSamples, ratioBuf.data(), snapper);

    // 4) Apply "amount" (0..1) and sibilant guard weighting
    float w = sibilant.weight(mono.data(), numSamples);
    const float amt = amount * w;
    for (int n=0;n<numSamples;++n){
        float r = ratioBuf[(size_t)n];
        ratioBuf[(size_t)n] = 1.0f + (r - 1.0f) * amt;
    }

    // 5) Build stereo wet buffer via shifter
    wetBuf.setSize(juce::jmax(2, numChans), numSamples, false, false, true);
    for (int ch=0; ch<wetBuf.getNumChannels(); ++ch)
        wetBuf.clear(ch, 0, numSamples);
    for (int ch=0; ch<numChans; ++ch){
        const float* in = buffer.getReadPointer(ch);
        float* wbuf = wetBuf.getWritePointer(ch);
        std::fill(tmpL.begin(), tmpL.begin()+numSamples, 0.0f);
        shifter.processBlock(in, tmpL.data(), numSamples, ratioBuf.data());
        std::copy(tmpL.begin(), tmpL.begin()+numSamples, wbuf);
    }

    // 6) Apply EMU color processing to the wet path only
    emu.process(wetBuf);

    // 7) Mix wet back into output
    for (int ch=0; ch<numChans; ++ch){
        float* out = buffer.getWritePointer(ch);
        const float* in = buffer.getReadPointer(ch);
        const float* w  = wetBuf.getReadPointer(ch);
        for (int n=0;n<numSamples;++n){
            out[n] = mix * w[n] + (1.0f - mix) * in[n];
        }
    }
}

juce::AudioProcessorEditor* HardTuneAudioProcessor::createEditor()
{
    return new HardTuneAudioProcessorEditor (*this);
}

void HardTuneAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream mos(destData, true);
    apvts.state.writeToStream(mos);
}

void HardTuneAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    juce::ValueTree tree = juce::ValueTree::readFromData(data, (size_t)sizeInBytes);
    if (tree.isValid())
        apvts.replaceState(tree);
    updateSnapper();
    updateRanges();
    updateColor();
}

juce::AudioProcessorValueTreeState::ParameterLayout HardTuneAudioProcessor::createParameterLayout()
{
    using P = juce::AudioParameterFloat;
    using C = juce::NormalisableRange<float>;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterChoice>("mode", "Mode", juce::StringArray("Track","Print"), 0));
    params.push_back(std::make_unique<P>("retune", "Retune Speed", C(0.0f, 1.0f, 0.001f), 1.0f));
    params.push_back(std::make_unique<P>("amount", "Correction Amount", C(0.0f, 1.0f, 0.001f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("key", "Key", juce::StringArray("C","C#","D","D#","E","F","F#","G","G#","A","A#","B"), 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("scale", "Scale", juce::StringArray("Chromatic","Major","Minor"), 1));
    params.push_back(std::make_unique<P>("color", "Color", C(0.0f, 1.0f, 0.001f), 0.15f));
    params.push_back(std::make_unique<P>("formant", "Formant Shift (st)", C(-12.0f, 12.0f, 0.01f), 0.0f));
    params.push_back(std::make_unique<P>("throat", "Throat Ratio", C(0.5f, 2.0f, 0.001f), 1.0f));
    params.push_back(std::make_unique<P>("mix", "Mix", C(0.0f, 1.0f, 0.001f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("bias", "Note Bias", juce::StringArray("Nearest","Up","Down"), 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>("inputType", "Input Type", juce::StringArray("Soprano","Alto","Tenor","Baritone","Bass"), 2));

    return { params.begin(), params.end() };
}
