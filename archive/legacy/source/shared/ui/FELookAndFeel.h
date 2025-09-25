#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

struct FELookAndFeel : juce::LookAndFeel_V4 {
    FELookAndFeel();

    // Tokens
    juce::Colour BG     { 0xFFE0E0E0 };
    juce::Colour PANEL  { 0xFFECECEC };
    juce::Colour TEXT   { 0xFF000000 };
    juce::Colour LABEL  { 0xFF6B6B6B };
    juce::Colour BORDER { 0xFF000000 };
    juce::Colour ACCENT { 0xFF00FF66 };
    juce::Colour ERRORC { 0xFFFF2B2B };

    int grid = 8;

    juce::Typeface::Ptr pixelTypeface;

    // Helpers
    void drawPixelBorder(juce::Graphics& g, juce::Rectangle<int> r, int px=1) const;
    void drawGrid8(juce::Graphics& g, juce::Rectangle<int> r) const;
    void titleBar(juce::Graphics& g, juce::Rectangle<int> r,
                  const juce::String& left, const juce::String& center, const juce::String& right) const;
};
