#pragma once
#include <JuceHeader.h>
#include "TemplePalette.h"

// Chunky, pixel-border LookAndFeel for sliders/buttons to sell the retro.
class TempleLookAndFeel : public juce::LookAndFeel_V4
{
public:
	TempleLookAndFeel();

	void drawButtonBackground (juce::Graphics&, juce::Button&,
							   const juce::Colour& backgroundColour,
							   bool shouldDrawButtonAsHighlighted,
							   bool shouldDrawButtonAsDown) override;

	void drawButtonText (juce::Graphics&, juce::TextButton&,
						 bool, bool) override;

	void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h,
						   float sliderPos, float min, float max,
						   const juce::Slider::SliderStyle, juce::Slider&) override;

	juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

private:
	void drawPixelFrame (juce::Graphics& g, juce::Rectangle<int> r,
						 juce::Colour main, juce::Colour edge, int thickness = 2);
};
