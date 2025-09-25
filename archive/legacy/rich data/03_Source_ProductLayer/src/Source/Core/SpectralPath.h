#pragma once
#include "../Spectral/STFTEngine.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace SpectralCanvas {

class SpectralPath {
public:
    void prepare(double sampleRate, int maxBlockSize, int numChannels){
        stft_.prepare(sampleRate, SpectralCanvas::spectral::STFTConfig{1024,256,1});
        in_.setSize(1, maxBlockSize, false, false, true);
        out_.setSize(1, maxBlockSize, false, false, true);
    }
    void reset(){ stft_.reset(); }
    
    bool isActive() const { return active_; }
    void setActive(bool active) { active_ = active; }

    void process(juce::AudioBuffer<float>& buffer){
        const int N = buffer.getNumSamples();
        in_.copyFrom(0, 0, buffer, 0, 0, N);
        out_.clear(0, 0, N);
        stft_.process(in_, out_, nullptr);
        buffer.copyFrom(0, 0, out_, 0, 0, N);
        if (buffer.getNumChannels() > 1)
            for (int ch=1; ch<buffer.getNumChannels(); ++ch)
                buffer.copyFrom(ch, 0, buffer, 0, 0, N);
    }

    void processBlock(juce::AudioBuffer<float>& buffer){
        process(buffer);
    }

private:
    SpectralCanvas::spectral::STFTEngine stft_;
    juce::AudioBuffer<float> in_, out_;
    bool active_ = true;
};

} // namespace SpectralCanvas
