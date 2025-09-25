#pragma once
#include <JuceHeader.h>
#include "UiStateProvider.h"
#include "TempleLookAndFeel.h"
#include "TempleWidgets.h"

class TempleEditor : public juce::AudioProcessorEditor,
					 private juce::Timer,
					 public juce::KeyListener
{
public:
	TempleEditor (UiStateProvider& provider);
	~TempleEditor() override;

	void paint (juce::Graphics&) override;
	void resized() override;

	// KeyListener
	bool keyPressed (const juce::KeyPress& key, juce::Component*) override;

private:
	UiStateProvider& state;
	TempleLookAndFeel lnf;

	Banner        banner;
	BandTable     bands;
	MasterPanel   master;
	ScanlineOverlay scan;
	CommandOverlay cmd;

	bool showScan { true };

	void timerCallback() override { repaint(); }
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TempleEditor)
};
