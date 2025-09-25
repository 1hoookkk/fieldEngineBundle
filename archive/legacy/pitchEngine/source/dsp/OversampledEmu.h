#pragma once
#include <juce_dsp/juce_dsp.h>
#include "AuthenticEMUZPlane.h"

// Minimal adapter: your AuthenticEMUZPlane must use this fs for theta scaling.
// Add a trivial setter in your EMU if you don't have one:
//   void setProcessingSampleRate(double fs){ fsHost = fs; }  // used only in coeff calc
struct EmuAdapter {
    AuthenticEMUZPlane& emu;
    double fs = 48000.0;
    void setSampleRate(double sr){ fs = sr; emu.setProcessingSampleRate(sr); }

    // In-place wrappers with zero allocation
    void processLinearInPlace(float* x, int N) {
        float* chans[] = { x };
        juce::AudioBuffer<float> buf (chans, 1, N); // non-owning view
        emu.processLinear(buf);
    }
    void processNonlinearInPlace(float* x, int N) {
        float* chans[] = { x };
        juce::AudioBuffer<float> buf (chans, 1, N); // non-owning view
        emu.processNonlinear(buf);
    }

    // params passthrough
    void setMorphPair(int pair){
        // Convert pair index to shape enum values
        auto shapeA = static_cast<AuthenticEMUZPlane::Shape>(pair % static_cast<int>(AuthenticEMUZPlane::Shape::NUM_SHAPES));
        auto shapeB = static_cast<AuthenticEMUZPlane::Shape>((pair + 1) % static_cast<int>(AuthenticEMUZPlane::Shape::NUM_SHAPES));
        emu.setShapePair(shapeA, shapeB);
    }
    void setMorphPosition(float p){ emu.setMorphPosition(p); }
    void setIntensity(float g){ emu.setIntensity(g); }
    void setDrive(float){ /* Note: AuthenticEMUZPlane handles drive internally */ }
};

class OversampledEmu {
public:
    enum class Mode { Off_1x, OS2_IIR, OS4_FIR }; // Track->Off_1x, Print->OS2 or OS4

    void prepare(double sampleRate, int numChannels, Mode m){
        fsBase = sampleRate; mode = m;
        const int factor = (mode == Mode::OS4_FIR ? 4 : (mode == Mode::OS2_IIR ? 2 : 1));
        const auto ftype = (mode == Mode::OS4_FIR
                            ? juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple // linear phase, more latency
                            : juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR); // min-phase, tiny latency
        if (factor == 1){
            oversampler.reset(); osFactor = 1; osLatency = 0;
        } else {
            oversampler = std::make_unique<juce::dsp::Oversampling<float>>(numChannels, juce::jmax(1, juce::roundToInt(std::log2((float)factor))), ftype);
            oversampler->reset();
            osFactor = factor;
            // FIR has latency; IIR is near-zero (we don't report it in Track)
            osLatency = oversampler->getLatencyInSamples();
        }
        // temp audio storage for upsampled processing
        upBlockBuffers.setSize(numChannels, (int)std::ceil((double)maxBlock * osFactor), false, false, true);
    }

    void setMaxBlock(int maxBlockSize){
        maxBlock = maxBlockSize;
        upBlockBuffers.setSize(upBlockBuffers.getNumChannels(), (int)std::ceil((double)maxBlock * juce::jmax(1, osFactor)), false, false, true);
    }

    // Call when mode changes (e.g., Track⇄Print). In Print you can add this latency to plugin PDC.
    int getLatencySamples() const { return osLatency; }
    int getFactor()        const { return osFactor; }

    // Run EMU oversampled in-place on 'buffer' (all channels). Params must be set on 'emu' beforehand.
    void process(EmuAdapter& emu, juce::AudioBuffer<float>& buffer, int numSamples){
        // Always do linear at base rate
        emu.setSampleRate(fsBase);
        for (int ch=0; ch<buffer.getNumChannels(); ++ch)
            emu.processLinearInPlace(buffer.getWritePointer(ch), numSamples);

        if (osFactor == 1) {
            // Nonlinear at base rate
            for (int ch=0; ch<buffer.getNumChannels(); ++ch)
                emu.processNonlinearInPlace(buffer.getWritePointer(ch), numSamples);
            return;
        }

        // 1) Up-sample for nonlinear only
        juce::dsp::AudioBlock<float> baseBlock (buffer.getArrayOfWritePointers(), (size_t)buffer.getNumChannels(), (size_t)numSamples);
        auto upBlock = oversampler->processSamplesUp(baseBlock);

        // 2) Process nonlinear at fsBase * osFactor
        emu.setSampleRate(fsBase * osFactor);
        for (int ch=0; ch<(int)upBlock.getNumChannels(); ++ch){
            float* x = upBlock.getChannelPointer(ch);
            emu.processNonlinearInPlace(x, (int)upBlock.getNumSamples());
        }

        // 3) Down-sample back in-place
        oversampler->processSamplesDown(baseBlock);
    }

private:
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    juce::AudioBuffer<float> upBlockBuffers;
    double fsBase = 48000.0;
    int osFactor = 1, osLatency = 0, maxBlock = 2048;
    Mode mode = Mode::Off_1x;
};