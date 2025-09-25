#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <atomic>

struct AtomicOscillatorParams
{
    std::atomic<float> frequency{440.0f};
    std::atomic<float> targetAmplitude{0.0f};

    AtomicOscillatorParams() = default;
    AtomicOscillatorParams(const AtomicOscillatorParams&) = delete;
    AtomicOscillatorParams& operator=(const AtomicOscillatorParams&) = delete;
};

class AtomicOscillator
{
public:
    AtomicOscillator() = default;
    AtomicOscillator(const AtomicOscillator&) = delete;
    AtomicOscillator& operator=(const AtomicOscillator&) = delete;

    void setSampleRate(float newSampleRate) noexcept
    {
        sampleRate_.store(newSampleRate, std::memory_order_release);
        updatePhaseIncrement(params_.frequency.load(std::memory_order_acquire));
    }

    void setFrequency(float freq) noexcept
    {
        params_.frequency.store(freq, std::memory_order_release);
        updatePhaseIncrement(freq);
    }

    void setTargetAmplitude(float target) noexcept { params_.targetAmplitude.store(target, std::memory_order_release); }
    void setSmoothingFactor(float factor) noexcept { smoothingFactor_.store(juce::jlimit(0.0f, 1.0f, factor), std::memory_order_release); }

    float generateSample() noexcept
    {
        float targetAmp = params_.targetAmplitude.load(std::memory_order_acquire);
        if (std::abs(currentAmplitude_ - targetAmp) > 0.0001f)
            currentAmplitude_ += (targetAmp - currentAmplitude_) * smoothingFactor_.load(std::memory_order_acquire);
        else
            currentAmplitude_ = targetAmp;

        float sample = std::sin(phase_) * currentAmplitude_;
        phase_ += phaseIncrement_.load(std::memory_order_acquire);
        if (phase_ >= juce::MathConstants<float>::twoPi) phase_ -= juce::MathConstants<float>::twoPi;
        return sample;
    }

    void reset() noexcept
    {
        params_.frequency.store(440.0f, std::memory_order_release);
        params_.targetAmplitude.store(0.0f, std::memory_order_release);
        phase_ = 0.0f;
        currentAmplitude_ = 0.0f;
        updatePhaseIncrement(440.0f);
    }

private:
    AtomicOscillatorParams params_{};
    std::atomic<float> sampleRate_{44100.0f};
    std::atomic<float> phaseIncrement_{0.0f};
    std::atomic<float> smoothingFactor_{0.05f};

    float phase_ = 0.0f;
    float currentAmplitude_ = 0.0f;

    void updatePhaseIncrement(float frequency) noexcept
    {
        float sr = sampleRate_.load(std::memory_order_acquire);
        float phaseInc = juce::MathConstants<float>::twoPi * frequency / juce::jmax(1.0f, sr);
        phaseIncrement_.store(phaseInc, std::memory_order_release);
    }
};
