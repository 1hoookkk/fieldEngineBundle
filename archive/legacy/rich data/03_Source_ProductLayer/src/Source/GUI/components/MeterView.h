#pragma once

#include <JuceHeader.h>

class MeterView : public juce::Component, private juce::Timer {
public:
	void setSource(juce::AudioBuffer<float>* buffer) { source_ = buffer; }
	void paint(juce::Graphics& g) override;
	void resized() override {}
	void start() { startTimerHz(30); }
	void stop() { stopTimer(); }
private:
	void timerCallback() override;
	juce::AudioBuffer<float>* source_ = nullptr;
	float rmsL_ = 0.0f, rmsR_ = 0.0f, peakL_ = 0.0f, peakR_ = 0.0f;
};


