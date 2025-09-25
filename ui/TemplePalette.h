#pragma once
#include <JuceHeader.h>

// 16-color VGA-ish palette (TempleOS vibe), intentionally jarring/high-contrast.
struct TemplePalette
{
	// Indexes approximate VGA: https://en.wikipedia.org/wiki/ANSI_escape_code#3-bit_and_4-bit
	static juce::Colour getColour(int index) {
		static const juce::Colour colors[16] = {
			juce::Colour(0x00,0x00,0x00),   // 0 black
			juce::Colour(0x00,0x00,0xAA),   // 1 blue
			juce::Colour(0x00,0xAA,0x00),   // 2 green
			juce::Colour(0x00,0xAA,0xAA),   // 3 cyan
			juce::Colour(0xAA,0x00,0x00),   // 4 red
			juce::Colour(0xAA,0x00,0xAA),   // 5 magenta
			juce::Colour(0xAA,0x55,0x00),   // 6 brown/orange
			juce::Colour(0xAA,0xAA,0xAA),   // 7 light gray
			juce::Colour(0x55,0x55,0x55),   // 8 dark gray
			juce::Colour(0x55,0x55,0xFF),   // 9 bright blue
			juce::Colour(0x55,0xFF,0x55),   // 10 bright green
			juce::Colour(0x55,0xFF,0xFF),   // 11 bright cyan
			juce::Colour(0xFF,0x55,0x55),   // 12 bright red
			juce::Colour(0xFF,0x55,0xFF),   // 13 bright magenta
			juce::Colour(0xFF,0xFF,0x55),   // 14 yellow
			juce::Colour(0xFF,0xFF,0xFF),   // 15 white
		};
		return colors[juce::jlimit(0, 15, index)];
	}
	
	// Legacy compatibility
	static juce::Colour col(int index) { return getColour(index); }

	static juce::Font mono (float size, juce::Font::FontStyleFlags style = juce::Font::plain)
	{
		return juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), size, style);
	}
};
