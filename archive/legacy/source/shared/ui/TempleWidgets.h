#pragma once
#include <JuceHeader.h>
#include <cmath>
#include "TemplePalette.h"
#include "UiStateProvider.h"

// CRT/scanline & noise overlay for screenshots
class ScanlineOverlay : public juce::Component, private juce::Timer
{
public:
	ScanlineOverlay() { startTimerHz (30); }
	void paint (juce::Graphics& g) override
	{
		auto b = getLocalBounds();
		// Dithered background: faint blue noise
		juce::Random r (seed);
		for (int i = 0; i < b.getWidth()*2; ++i)
		{
			auto x = r.nextInt (b.getWidth());
			auto y = r.nextInt (b.getHeight());
			if ((x + y + seed) % 7 == 0)
				g.setColour (TemplePalette::col(1).withAlpha (0.12f));
			else
				g.setColour (TemplePalette::col(8).withAlpha (0.04f));
			g.fillRect (x, y, 1, 1);
		}

		// Scanlines
		for (int y = 0; y < b.getHeight(); y += 2)
		{
			g.setColour (juce::Colours::black.withAlpha (0.12f));
			g.fillRect (0, y, b.getWidth(), 1);
		}
	}
private:
	void timerCallback() override { ++seed; repaint(); }
	int seed { 0 };
};

// Big retro banner with double-line box-drawing
class Banner : public juce::Component
{
public:
	Banner (UiStateProvider& s) : state (s) {}
	void paint (juce::Graphics& g) override
	{
		auto r = getLocalBounds();
		g.fillAll (TemplePalette::col(0));

		// Frame
		g.setColour (TemplePalette::col(15));
		g.drawRect (r, 4);
		r.reduce (6, 6);

		// Header bar
		g.setColour (TemplePalette::col(4));
		g.fillRect (r.removeFromTop (32));
		g.setColour (TemplePalette::col(15));
		g.setFont (TemplePalette::mono (20.f, juce::Font::bold));
		g.drawText ("=≡= " + state.getTitle() + " =≡=", getLocalBounds().withTrimmedTop(2).removeFromTop(32),
					juce::Justification::centred);

		// Subtitle
		g.setFont (TemplePalette::mono (12.f));
		g.setColour (TemplePalette::col(11));
		g.drawText ("REAL-TIME Z•PLANE LAB  // F1: HELP  // `: CONSOLE // [G]ATEKEEP MODE",
					getLocalBounds().withTrimmedTop(36).removeFromTop(18),
					juce::Justification::centred);
	}
private:
	UiStateProvider& state;
};

// Per-band table: harsh grid, ANSI bars for energy, morph alpha readout
class BandTable : public juce::Component, private juce::Timer
{
public:
	BandTable (UiStateProvider& s) : state (s) { startTimerHz (30); }

	void paint (juce::Graphics& g) override
	{
		auto b = getLocalBounds();
		g.fillAll (TemplePalette::col(0));
		g.setColour (TemplePalette::col(15));
		g.drawRect (b, 3);

		auto header = b.removeFromTop (22);
		g.setFont (TemplePalette::mono (13.f, juce::Font::bold));
		g.setColour (TemplePalette::col(14));
		g.drawText ("BAND   ENERGY      MORPH α   GAIN    PATH", header, juce::Justification::centredLeft);
		g.setColour (TemplePalette::col(15));
		g.drawLine ((float)b.getX(), (float)b.getY(), (float)b.getRight(), (float)b.getY(), 2.f);

		const int rows = juce::jlimit (0, 16, state.getNumBands());
		auto rowH = juce::jmax (18, b.getHeight() / juce::jmax (1, rows));

		for (int i = 0; i < rows; ++i)
		{
			auto row = b.removeFromTop (rowH);
			drawRow (g, row, i);
			g.setColour (TemplePalette::col(1));
			g.fillRect (row.removeFromBottom (1));
		}
	}

private:
	UiStateProvider& state;

	void drawRow (juce::Graphics& g, juce::Rectangle<int> row, int i)
	{
		auto lhs = row.removeFromLeft (52);
		g.setColour (TemplePalette::col(3));
		g.setFont (TemplePalette::mono (12.f, juce::Font::bold));
		juce::String name = state.getBandName (i);
		if (name.isEmpty()) name = "B" + juce::String (i+1);
		g.drawText (name, lhs, juce::Justification::centredLeft);

		// Energy bar (ANSI blocks)
		auto energyR = row.removeFromLeft (160).reduced (4, 4);
		auto e = juce::jlimit (0.f, 1.f, state.getBandEnergy (i));
		const int cells = 20;
		const int filled = (int)juce::roundToInt (e * cells);
		for (int c = 0; c < cells; ++c)
		{
			auto cell = energyR.removeFromLeft (energyR.getWidth() / juce::jmax (1, cells));
			g.setColour (c < filled ? TemplePalette::col(10) : TemplePalette::col(8));
			g.fillRect (cell.reduced (1));
		}

		// Morph α
		auto morphR = row.removeFromLeft (100);
		g.setColour (TemplePalette::col(13));
		g.drawText (juce::String (state.getBandMorphAlpha (i), 2), morphR, juce::Justification::centredLeft);

		// Gain
		auto gainR = row.removeFromLeft (80);
		g.setColour (TemplePalette::col(12));
		auto db = state.getBandGainDb (i);
		g.drawText (juce::String (db, 1) + " dB", gainR, juce::Justification::centredLeft);

		// Path (ASCII)
		g.setColour (TemplePalette::col(11));
		g.drawText (state.getBandMorphPath (i), row, juce::Justification::centredLeft);

		// Mute badge
		if (state.isBandMuted (i))
		{
			auto m = row.removeFromRight (40).reduced (2);
			g.setColour (TemplePalette::col(4));
			g.fillRect (m);
			g.setColour (TemplePalette::col(15));
			g.setFont (TemplePalette::mono (11.f, juce::Font::bold));
			g.drawText ("MUTE", m, juce::Justification::centred);
		}
	}

	void timerCallback() override { repaint(); }
};

// Master panel: a few big toggles + master morph scope (thin!)
class MasterPanel : public juce::Component, private juce::Timer
{
public:
	MasterPanel (UiStateProvider& s) : state (s) { startTimerHz (30); }

	void paint (juce::Graphics& g) override
	{
		auto r = getLocalBounds();
		g.fillAll (TemplePalette::col(0));
		g.setColour (TemplePalette::col(15)); g.drawRect (r, 3);
		r.reduce (8, 8);

		// Scope
		auto scope = r.removeFromTop (r.getHeight() * 0.5f);
		drawScope (g, scope);

		// Toggles (fake-ish; you can replace w/ actual Buttons if desired)
		auto row = r.removeFromTop (28);
		drawToggle (g, row.removeFromLeft (120), "BYPASS", state.isBypassed());
		drawToggle (g, row.removeFromLeft (160), "SIDECHAIN", state.isSidechainActive());
		drawToggle (g, row.removeFromLeft (200), "GATEKEEP", gatekeep);
		// Hint text
		g.setColour (TemplePalette::col(7));
		g.setFont (TemplePalette::mono (12.f));
		g.drawText ("Press ` to open CONSOLE • Press G to toggle Gatekeep", r, juce::Justification::centredLeft);
	}

	void toggleGatekeep() { gatekeep = !gatekeep; repaint(); }

private:
	UiStateProvider& state;
	bool gatekeep { true };

	void drawScope (juce::Graphics& g, juce::Rectangle<int> area)
	{
		g.setColour (TemplePalette::col(1)); g.fillRect (area);
		g.setColour (TemplePalette::col(15)); g.drawRect (area, 2);

		// grid
		g.setColour (TemplePalette::col(8));
		for (int i = 1; i < 8; ++i)
			g.drawLine ((float)area.getX() + area.getWidth() * (i / 8.f), (float)area.getY(),
						(float)area.getX() + area.getWidth() * (i / 8.f), (float)area.getBottom(), 1.0f);

		// master morph alpha waveform (fake LFO-looking reading)
		auto a = juce::jlimit (0.f, 1.f, state.getMasterMorphAlpha());
		juce::Path p;
		const int N = juce::jmax (64, area.getWidth());
		for (int i = 0; i < N; ++i)
		{
			float t = (float)i / (N - 1);
			float y = 0.5f + 0.45f * std::sin (6.28318f * (t + a * 0.25f));
			float px = (float)area.getX() + t * area.getWidth();
			float py = (float)area.getY() + (1.f - y) * area.getHeight();
			if (i == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
		}
		g.setColour (TemplePalette::col(10));
		g.strokePath (p, juce::PathStrokeType (2.0f));
	}

	void drawToggle (juce::Graphics& g, juce::Rectangle<int> box, const juce::String& label, bool on)
	{
		g.setColour (on ? TemplePalette::col(10) : TemplePalette::col(8));
		g.fillRect (box);
		g.setColour (TemplePalette::col(15)); g.drawRect (box, 3);
		g.setFont (TemplePalette::mono (13.f, juce::Font::bold));
		g.drawText (label, box, juce::Justification::centred);
	}

	void timerCallback() override { repaint(); }
};

// Command overlay (press ` or F1): “gatekept” feel, shows cheats/hidden ops
class CommandOverlay : public juce::Component
{
public:
	void setVisibleAnimated (bool v) { setVisible (v); repaint(); }
	bool keyPressed (const juce::KeyPress& k)
	{
		if (k.getTextCharacter() == juce::KeyPress::escapeKey) { setVisibleAnimated (false); return true; }
		return false;
	}

	void paint (juce::Graphics& g) override
	{
		if (! isVisible()) return;
		auto r = getLocalBounds();
		g.setColour (juce::Colours::black.withAlpha (0.65f)); g.fillRect (r);

		auto p = r.reduced (40);
		g.setColour (TemplePalette::col(0)); g.fillRect (p);
		g.setColour (TemplePalette::col(15)); g.drawRect (p, 4);

		p.reduce (10, 10);
		g.setFont (TemplePalette::mono (18.f, juce::Font::bold));
		g.setColour (TemplePalette::col(14));
		g.drawText ("COMMAND CONSOLE", p.removeFromTop (28), juce::Justification::centred);

		g.setFont (TemplePalette::mono (13.f));
		g.setColour (TemplePalette::col(11));
		g.drawFittedText(
			"Hidden Ops (Gatekeep Mode):\n"
			"  :band <n> mute|solo|boost <dB>\n"
			"  :morph master <0..1>\n"
			"  :style <breaks|techno|lofi|dj>\n"
			"  :randomize hi-only\n"
			"  :panic\n"
			"\nPress ESC to close.",
			p.reduced (8), juce::Justification::topLeft, 20);
	}
};
