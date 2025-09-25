#include "SoundRenderer.h"
#include <cmath>

void SoundRenderer::renderFromCanvas(const std::vector<PaintStroke>& strokes,
    juce::AudioBuffer<float>& buffer,
    int sampleRate,
    float durationSeconds)
{
    int totalSamples = static_cast<int>(sampleRate * durationSeconds);
    buffer.setSize(1, totalSamples);
    buffer.clear();

    for (const auto& stroke : strokes)
    {
        int startSample = static_cast<int>(stroke.timeNorm * totalSamples);
        float freq = juce::jmap(stroke.freqNorm, 0.0f, 1.0f, 50.0f, 5000.0f);
        float amp = 0.3f * stroke.pressure; // Use pressure for amplitude

        for (int i = 0; i < 2000; ++i)
        {
            int idx = startSample + i;
            if (idx >= totalSamples) break;
            float sample = amp * std::sin(2.0f * juce::MathConstants<float>::pi * freq * i / (float)sampleRate);
            buffer.setSample(0, idx, buffer.getSample(0, idx) + sample);
        }
    }
}
