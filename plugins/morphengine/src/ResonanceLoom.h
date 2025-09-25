#pragma once

#include <array>
#include <vector>
#include <atomic>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "../../../EMUFilter.h"
#include "MorphEngineAudioProcessor.h"

class ResonanceMagnitudeSampler
{
public:
    ResonanceMagnitudeSampler();

    void setSampleRate (double newSampleRate) noexcept;
    void setFrequencyRange (float minHz, float maxHz) noexcept;
    void setNumBins (int bins);

    bool compute (const std::array<AuthenticEMUZPlane::BiquadCoeffs, 6>& coeffs);

    const std::vector<float>& getFrequencies() const noexcept { return frequencies; }
    const std::vector<float>& getTotalMagnitudes() const noexcept { return totalMagnitudes; }
    const std::vector<float>& getSectionMagnitudes() const noexcept { return sectionMagnitudes; }
    int getNumSections() const noexcept { return numSections; }

private:
    void rebuildFrequencyTable();

    double sampleRate = 48000.0;
    float minFrequency = 20.0f;
    float maxFrequency = 20000.0f;
    int numBins = 1024;
    int numSections = 6;
    bool frequencyTableDirty = true;

    std::vector<float> frequencies;
    std::vector<float> sectionMagnitudes;
    std::vector<float> totalMagnitudes;
};

class ResonanceLoom : public juce::Component,
                      private juce::Timer
{
public:
    explicit ResonanceLoom (MorphEngineAudioProcessor& processorRef);
    ~ResonanceLoom() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    void setBins (int bins);
    void setFrequencyRange (float minHz, float maxHz);

private:
    void timerCallback() override;
    void pullFrame();
    void drawBackground (juce::Graphics& g, const juce::Rectangle<float>& bounds) const;
    void drawSections (juce::Graphics& g, const juce::Rectangle<float>& bounds);
    void drawTotal (juce::Graphics& g, const juce::Rectangle<float>& bounds);
    juce::Colour getSectionColour (int index) const;
    void drawModeLegend (juce::Graphics& g, const juce::Rectangle<float>& bounds);
    void resetSmoothingBuffers();

    MorphEngineAudioProcessor& processor;
    ResonanceMagnitudeSampler sampler;

    MorphEngineAudioProcessor::FilterFrame frame {};
    bool frameValid = false;

    std::vector<float> smoothedSections;
    std::vector<float> smoothedTotal;
    float smoothing = 0.75f;
    double currentSampleRate = 48000.0;
    int targetBins = 1024;
    double lastComputeMs = 0.0;
    juce::int64 lastAdaptTicks = 0;
    bool timerStarted = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ResonanceLoom)
};

