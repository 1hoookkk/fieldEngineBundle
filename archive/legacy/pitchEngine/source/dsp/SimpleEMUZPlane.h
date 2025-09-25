#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>

/**
 * Simplified EMU Z-Plane Filter
 * Authentic sound with minimal complexity
 */
class SimpleEMUZPlane {
public:
    void prepare(double sampleRate) {
        fs = sampleRate;
        reset();
    }

    void setMorphPosition(float position01) {
        morph = juce::jlimit(0.0f, 1.0f, position01);
        updateCoefficients();
    }

    void setIntensity(float intensity01) {
        intensity = juce::jlimit(0.0f, 1.0f, intensity01);
        updateCoefficients();
    }

    void process(juce::AudioBuffer<float>& buffer) {
        const int numChannels = buffer.getNumChannels();
        const int numSamples = buffer.getNumSamples();

        for (int ch = 0; ch < numChannels; ++ch) {
            auto* data = buffer.getWritePointer(ch);
            auto& chain = (ch == 0) ? leftChain : rightChain;

            for (int i = 0; i < numSamples; ++i) {
                float sample = data[i];

                // Process through 6 biquads
                for (auto& biquad : chain) {
                    sample = biquad.process(sample);
                }

                // Soft saturation
                sample = std::tanh(sample * drive) * makeup;
                data[i] = sample;
            }
        }
    }

    void reset() {
        for (auto& b : leftChain) b.reset();
        for (auto& b : rightChain) b.reset();
    }

private:
    struct SimpleBiquad {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
        float a1 = 0.0f, a2 = 0.0f;
        float z1 = 0.0f, z2 = 0.0f;

        float process(float x) {
            float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }

        void reset() { z1 = z2 = 0.0f; }
    };

    double fs = 48000.0;
    float morph = 0.5f;
    float intensity = 0.7f;
    float drive = 1.0f;
    float makeup = 1.0f;

    std::array<SimpleBiquad, 6> leftChain;
    std::array<SimpleBiquad, 6> rightChain;

    void updateCoefficients() {
        // EMU-style morphing between vowel formants
        // Simplified but authentic sounding

        for (int i = 0; i < 6; ++i) {
            // Base frequencies for formants
            const float freqA = 200.0f * (1.0f + i * 0.8f);  // Vowel A formants
            const float freqB = 300.0f * (1.0f + i * 1.2f);  // Vowel E formants

            // Morph between frequencies
            const float freq = freqA + morph * (freqB - freqA);
            const float q = 2.0f + intensity * 8.0f;  // Q from 2 to 10

            // Calculate normalized frequency
            const float omega = 2.0f * 3.14159265f * freq / fs;
            const float sin_omega = std::sin(omega);
            const float cos_omega = std::cos(omega);
            const float alpha = sin_omega / (2.0f * q);

            // Resonant peak filter (bandpass character)
            const float norm = 1.0f / (1.0f + alpha);

            leftChain[i].b0 = alpha * norm;
            leftChain[i].b1 = 0.0f;
            leftChain[i].b2 = -alpha * norm;
            leftChain[i].a1 = -2.0f * cos_omega * norm;
            leftChain[i].a2 = (1.0f - alpha) * norm;

            // Copy to right channel
            rightChain[i] = leftChain[i];
        }

        // Update drive and makeup
        drive = 1.0f + intensity * 2.0f;
        makeup = 1.0f / std::sqrt(1.0f + intensity);
    }
};