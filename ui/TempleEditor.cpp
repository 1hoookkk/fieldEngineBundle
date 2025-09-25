#include "TempleEditor.h"
#include "TemplePalette.h"

TempleEditor::TempleEditor (UiStateProvider& provider)
	: juce::AudioProcessorEditor (dynamic_cast<juce::AudioProcessor*>(&provider))
	, state (provider)
	, banner (state)
	, bands (state)
	, master (state)
{
	setLookAndFeel (&lnf);
	setOpaque (true);
	setWantsKeyboardFocus (true);
	addKeyListener (this);

	addAndMakeVisible (banner);
	addAndMakeVisible (bands);
	addAndMakeVisible (master);
	addAndMakeVisible (cmd);
	addAndMakeVisible (scan);

	// Tall default, screenshot-friendly. Resize as you like.
	setSize (860, 560);
	startTimerHz (30);
}

TempleEditor::~TempleEditor()
{
	setLookAndFeel (nullptr);
}

void TempleEditor::paint (juce::Graphics& g)
{
	g.fillAll (TemplePalette::col(0));

	// Bottom status bar
	auto sb = getLocalBounds().removeFromBottom (22);
	g.setColour (TemplePalette::col(5)); g.fillRect (sb);
	g.setColour (TemplePalette::col(15));
	g.setFont (TemplePalette::mono (12.f));
	auto info = juce::String::formatted(
		"SR: %.0f  |  MASTER α: %.2f  |  %s  |  F1: HELP  `: CONSOLE  G: GATEKEEP  S: SCANLINES",
		state.getSampleRate(), (double)state.getMasterMorphAlpha(),
		state.isSidechainActive() ? "SIDECHAIN:ON" : "SIDECHAIN:OFF");
	g.drawText (info, sb.reduced (6), juce::Justification::centredLeft);

	if (! showScan) scan.setVisible (false);
	else            scan.setVisible (true);
}

void TempleEditor::resized()
{
	auto r = getLocalBounds();
	auto status = r.removeFromBottom (22);
	juce::ignoreUnused (status);

	banner.setBounds (r.removeFromTop (64));
	auto main = r.reduced (8);

	auto left = main.removeFromLeft (juce::jmax (300, main.getWidth() * 3 / 5));
	bands.setBounds (left);

	main.removeFromLeft (8);
	master.setBounds (main);

	cmd.setBounds (getLocalBounds());
	scan.setBounds (getLocalBounds());
}

bool TempleEditor::keyPressed (const juce::KeyPress& k, juce::Component*)
{
	if (k.getKeyCode() == juce::KeyPress::F1Key || k.getTextCharacter() == '`')
	{
		cmd.setVisibleAnimated (! cmd.isVisible());
		return true;
	}
	if (k.getTextCharacter() == 's' || k.getTextCharacter() == 'S')
	{
		showScan = ! showScan; repaint();
		return true;
	}
	if (k.getTextCharacter() == 'g' || k.getTextCharacter() == 'G')
	{
		master.toggleGatekeep();
		return true;
	}
	return cmd.keyPressed (k); // allow ESC to close
}
