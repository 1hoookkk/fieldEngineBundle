// Core/SpectralProcessor.h
#pragma once
#include <JuceHeader.h>

class SpectralProcessor
{
public:
    SpectralProcessor();
    
    // FFT operations
    void analyzeBuffer(const juce::AudioBuffer<float>& input);
    void resynthesizeToBuffer(juce::AudioBuffer<float>& output);
    
    // CDP-style transformations
    void spectralMask(const FrequencyMask& mask);
    void spectralStretch(float factor);
    void spectralShift(float semitones);
    void morphSpectra(const SpectralData& target, float amount);
    
    // Granular operations
    void granularScatter(const GrainParameters& params);
    void timeStretchGranular(float factor);

private:
    static constexpr int fftOrder = 11; // 2048 samples
    juce::dsp::FFT forwardFFT{fftOrder};
    juce::dsp::FFT inverseFFT{fftOrder};
    
    std::vector<std::complex<float>> frequencyDomain;
    std::vector<float> magnitudes;
    std::vector<float> phases;
};