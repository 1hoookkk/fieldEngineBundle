#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
struct Shifter {
    void prepare(double,int,int){}
    void process(juce::AudioBuffer<float>&, const float* ratio, const float* weight)
    {
        juce::ignoreUnused(ratio, weight);
    }
};