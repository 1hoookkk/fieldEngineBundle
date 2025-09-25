#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <array>

class MorphFilter
{
public:
    MorphFilter() = default;
    ~MorphFilter() = default;

    void prepare(double sampleRate, int maxBlockSize);
    void reset();
    void process(juce::AudioBuffer<float>& buffer);

    void setMorph(float value) noexcept { morph.setTargetValue(juce::jlimit(0.0f, 1.0f, value)); }
    void setCutoff(float hz) noexcept { cutoff.setTargetValue(juce::jlimit(20.0f, 20000.0f, hz)); }
    void setResonance(float q) noexcept { resonance.setTargetValue(juce::jlimit(0.1f, 10.0f, q)); }
    void setDrive(float db) noexcept { drive.setTargetValue(juce::jlimit(0.0f, 24.0f, db)); }
    void setPrePost(bool pre) noexcept { preMode = pre; }

private:
    struct SVFState { float z1 = 0.0f; float z2 = 0.0f; };
    struct Coefficients { float g = 0.0f; float k = 0.0f; float a1 = 0.0f; float a2 = 0.0f; float a3 = 0.0f; };

    Coefficients coeffs;
    std::array<SVFState, 2> channelStates{};

    juce::LinearSmoothedValue<float> morph, cutoff, resonance, drive;

    double sampleRate = 44100.0;
    bool preMode = true;

    void updateCoefficients();
    float processSample(float input, int channel);
    float blendResponses(float lowpass, float bandpass, float highpass) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MorphFilter)
};
