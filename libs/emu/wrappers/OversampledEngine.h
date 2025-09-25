#pragma once
#include "api/IZPlaneEngine.h"
#include <juce_dsp/juce_dsp.h>
#include <vector>

class OversampledEngine
{
public:
    enum class Mode { Off_1x, OS2_IIR, OS4_FIR };

    void prepare (double fs, int numCh, Mode m)
    {
        fsBase = fs;  mode = m;
        const int desired = (m == Mode::Off_1x ? 1 : m == Mode::OS2_IIR ? 2 : 4);

        if (desired == 1)
        {
            oversampler.reset();
            latencySamples = 0;
            return;
        }

        const auto ft = (desired == 2)
            ? juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR
            : juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple;

        // JUCE constructor expects exponent: 2^exp. Map desired factor -> exponent.
        const size_t exp = (desired == 2 ? 1u : 2u);
        oversampler = std::make_unique<juce::dsp::Oversampling<float>> ((size_t) numCh,
                                                                        exp,
                                                                        ft,
                                                                        true,   // max quality
                                                                        false); // integer latency off
        oversampler->reset();
        latencySamples = (int) oversampler->getLatencyInSamples();
    }

    void setMaxBlock (int n)
    {
        if (oversampler)
            oversampler->initProcessing ((size_t) n);
    }

    int  getLatencySamples() const { return latencySamples; }
    Mode getMode()           const { return mode; }

    // Linear @ base rate; Nonlinear wrapped in OS island (if any).
    void process (IZPlaneEngine& engine, juce::AudioBuffer<float>& wet, int n)
    {
        engine.setProcessingSampleRate (fsBase);
        engine.processLinear (wet);

        if (!oversampler) { engine.processNonlinear (wet); return; }

        juce::dsp::AudioBlock<float> base (wet.getArrayOfWritePointers(),
                                           (size_t)wet.getNumChannels(), (size_t)n);

        juce::dsp::AudioBlock<const float> in (wet.getArrayOfReadPointers(),
                                               (size_t)wet.getNumChannels(), (size_t)n);

        auto upBlock = oversampler->processSamplesUp (in);

        const int factor = (int) oversampler->getOversamplingFactor();
        engine.setProcessingSampleRate (fsBase * factor);

        // Wrap the upsampled block into an AudioBuffer view for the engine
        std::vector<float*> chPtrs ((size_t) upBlock.getNumChannels());
        for (size_t c = 0; c < upBlock.getNumChannels(); ++c)
            chPtrs[c] = upBlock.getChannelPointer (c);
        juce::AudioBuffer<float> upView (chPtrs.data(), (int) upBlock.getNumChannels(), (int) upBlock.getNumSamples());
        engine.processNonlinear (upView);

        oversampler->processSamplesDown (base);
        engine.setProcessingSampleRate (fsBase);
    }

private:
    double fsBase = 48000.0;
    int    latencySamples = 0;
    Mode   mode = Mode::Off_1x;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
};
