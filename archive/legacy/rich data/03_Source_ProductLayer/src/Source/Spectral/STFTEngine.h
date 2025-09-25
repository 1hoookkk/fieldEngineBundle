#pragma once
#include <juce_audio_basics/juce_audio_basics.h>

namespace SpectralCanvas {
namespace spectral {

struct STFTConfig {
    int fftSize;
    int hopSize;
    int channels;
};

class STFTEngine {
public:
    STFTEngine() = default;
    ~STFTEngine() = default;
    
    void prepare(double sampleRate, const STFTConfig& config);
    void reset();
    void process(const juce::AudioBuffer<float>& input, 
                 juce::AudioBuffer<float>& output,
                 void* userData);

private:
    STFTConfig config_{1024, 256, 1};
    double sampleRate_ = 44100.0;
    bool initialized_ = false;
};

} // namespace spectral  
} // namespace SpectralCanvas
