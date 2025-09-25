#include "PluginMinimal/PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include "UIMinimal/SpectralCanvas.h"

class MinimalEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit MinimalEditor (SpectralCanvasProMinimalProcessor& p)
    : juce::AudioProcessorEditor(p), proc_(p), canvas_(p.brushQueue())
    {
        setSize (800, 360);
        addAndMakeVisible(canvas_);
        canvas_.enableGPU(true);
        startTimerHz(30); // UI polling (outside RT)
    }
    void resized() override { canvas_.setBounds(getLocalBounds()); }
private:
    void timerCallback() override {
        canvas_.setMagnitudesForUI(proc_.magnitudesForUI(), proc_.maskSnapshotForUI());
    }
    SpectralCanvasProMinimalProcessor& proc_;
    SpectralCanvas canvas_;
};

SpectralCanvasProMinimalProcessor::SpectralCanvasProMinimalProcessor() {
    stft_.setLatencyPolicyCentered(); // Minimal target policy
}

void SpectralCanvasProMinimalProcessor::prepareToPlay (double sr, int maxBlock) {
    stft_.prepare(sr, maxBlock, cfg_);
    mask_.configure(/*frames*/ 512, /*bins*/ cfg_.fftSize/2 + 1);
    setLatencySamples(stft_.reportedLatencySamples());
    // Z-Plane: prepare and set hop-based smoothing
    stft_.prepareZPlane(sr, cfg_.fftSize, cfg_.fftSize/2 + 1, cfg_.hop);
}

void SpectralCanvasProMinimalProcessor::processBlock (juce::AudioBuffer<float>& buf, juce::MidiBuffer&) {
    juce::ScopedNoDenormals _;
    auto* in  = buf.getReadPointer (0);
    auto* out = buf.getWritePointer(0);
    stft_.process(in, out, buf.getNumSamples(), mask_, brushQ_, 16);

    // mono->other channels
    for (int ch = 1; ch < buf.getNumChannels(); ++ch)
        buf.copyFrom(ch, 0, buf, 0, 0, buf.getNumSamples());
}

juce::AudioProcessorEditor* SpectralCanvasProMinimalProcessor::createEditor() {
    return new MinimalEditor(*this);
}