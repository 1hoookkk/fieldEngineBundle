#include "TempleLookAndFeel.h"

TempleLookAndFeel::TempleLookAndFeel()
{
	setColour (juce::TextButton::buttonColourId, TemplePalette::col(1));
	setColour (juce::TextButton::buttonOnColourId, TemplePalette::col(4));
	setColour (juce::TextButton::textColourOnId, TemplePalette::col(15));
	setColour (juce::TextButton::textColourOffId, TemplePalette::col(15));

	setColour (juce::Slider::backgroundColourId, TemplePalette::col(8));
	setColour (juce::Slider::thumbColourId, TemplePalette::col(14));
	setColour (juce::Slider::trackColourId, TemplePalette::col(9));
}

juce::Font TempleLookAndFeel::getTextButtonFont (juce::TextButton&, int h)
{
	return TemplePalette::mono (juce::jmax (12, h - 8), juce::Font::bold);
}

void TempleLookAndFeel::drawPixelFrame (juce::Graphics& g, juce::Rectangle<int> r,
											juce::Colour main, juce::Colour edge, int t)
{
	g.setColour (edge);
	g.drawRect (r, t);
	r.reduce (t, t);
	g.setColour (main);
	g.drawRect (r, 1);
}

void TempleLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b,
												  const juce::Colour& bg,
												  bool hl, bool down)
{
	auto r = b.getLocalBounds().reduced (2);
	auto main = down ? TemplePalette::col(4) : (hl ? TemplePalette::col(9) : bg);
	auto edge = TemplePalette::col(15);

	g.fillAll (TemplePalette::col(0));
	g.setColour (main);
	g.fillRect (r);

	drawPixelFrame (g, r, main.brighter (0.2f), edge, 3);
}

void TempleLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b,
											 bool, bool)
{
	g.setColour (TemplePalette::col(15));
	g.setFont (getTextButtonFont (b, b.getHeight()));
	g.drawFittedText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, 1);
}

void TempleLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h,
											  float sliderPos, float, float,
											  const juce::Slider::SliderStyle, juce::Slider&)
{
	auto r = juce::Rectangle<int> (x, y, w, h).reduced (2);
	g.fillAll (TemplePalette::col(0));

	// track
	auto track = r.reduced (6, h > w ? 10 : 4);
	g.setColour (TemplePalette::col(9));
	g.fillRect (track);

	// thumb as chunky bar
	g.setColour (TemplePalette::col(14));
	if (h > w)
		g.fillRect (track.withY ((int)sliderPos - 4).withHeight (8));
	else
		g.fillRect (track.withX ((int)sliderPos - 4).withWidth (8));

	// pixel frame
	drawPixelFrame (g, r, TemplePalette::col(8), TemplePalette::col(7), 3);
}
