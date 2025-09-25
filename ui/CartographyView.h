#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <span>
#include "../dsp/MorphEngine.h"
#include "FELookAndFeel.h"

class CartographyView : public juce::Component, private juce::Timer {
public:
    CartographyView();

    enum Mode { Waveform, Spectrum, VectorField };
    void setMode(Mode m);

    // Parameter hooks from Editor
    void setDriveDb(float dB);
    void setFocus01(float f);
    void setContour(float c);

    // Audio telemetry (drained from FIFO by editor)
    void pushMonoSamples(std::span<const float> mono);
    void setTelemetry(const fe::MorphEngine::Telemetry& t);

    void paint(juce::Graphics&) override;
    void resized() override {}

private:
    Mode mode = Waveform;
    float driveDb = 0.f, focus01 = 0.7f, contour = 0.f;

    std::vector<float> history; // simple ring buffer for waveform
    size_t idx = 0;

    fe::MorphEngine::Telemetry lastTel{};
    juce::int64 lastChangeMs = 0; // for 2s ghost

    void timerCallback() override { repaint(); }
    void drawFrame(juce::Graphics& g, juce::Rectangle<int> r);
    void drawGrid(juce::Graphics& g, juce::Rectangle<int> r);
    void drawWaveform(juce::Graphics& g, juce::Rectangle<int> r);
    void drawSpectrum(juce::Graphics& g, juce::Rectangle<int> r);
    void drawVector(juce::Graphics& g, juce::Rectangle<int> r);
};
