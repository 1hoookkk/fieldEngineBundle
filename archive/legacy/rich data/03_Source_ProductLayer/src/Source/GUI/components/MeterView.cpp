#include "MeterView.h"

using juce::Rectangle;
using juce::Colour;

void MeterView::timerCallback()
{
	if (!source_) return;
	const int n = source_->getNumSamples();
	const int ch = source_->getNumChannels();
	auto calc = [&](int c)->std::pair<float,float>{
		if (c >= ch) return {0.0f, 0.0f};
		auto* d = source_->getReadPointer(c);
		float sum = 0.0f, pk = 0.0f;
		for (int i = 0; i < n; ++i) { const float v = d[i]; sum += v*v; pk = std::max(pk, std::abs(v)); }
		const float rms = std::sqrt(std::max(0.0f, sum / std::max(1, n)));
		return {rms, pk};
	};
	std::tie(rmsL_, peakL_) = calc(0);
	std::tie(rmsR_, peakR_) = calc(1);
	repaint();
}

void MeterView::paint(juce::Graphics& g)
{
	auto area = getLocalBounds().toFloat();
	auto left = area.removeFromLeft(area.getWidth() * 0.5f);
	auto drawBar = [&](juce::Rectangle<float> r, float v, Colour c){
		g.setColour(c.withAlpha(0.15f)); g.fillRect(r);
		auto h = juce::jlimit(0.0f, 1.0f, v) * r.getHeight();
		g.setColour(c); g.fillRect(r.removeFromBottom(h));
	};
	drawBar(left.reduced(2), peakL_, juce::Colours::lime);
	drawBar(area.reduced(2), peakR_, juce::Colours::lime);
}


