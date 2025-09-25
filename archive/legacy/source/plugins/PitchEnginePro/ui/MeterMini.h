#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

// Lean stereo RMS meter with clip tint; ~30 FPS from a timer.
class MeterMini : public juce::Component, private juce::Timer {
public:
    void setLevels (float l, float r, bool clipL, bool clipR) {
        levelL = l; levelR = r; clippedL = clipL; clippedR = clipR;
    }
    void start() { startTimerHz (30); }
    void stop()  { stopTimer(); }

    void paint (juce::Graphics& g) override {
        auto b = getLocalBounds().toFloat();
        auto half = b.removeFromLeft (b.getWidth() * 0.5f);
        drawBar (g, half, levelL, clippedL);
        drawBar (g, b,     levelR, clippedR);
    }

private:
    void timerCallback() override { repaint(); }
    void drawBar (juce::Graphics& g, juce::Rectangle<float> r, float v, bool clipped) {
        g.setColour (theme::c(theme::panel));
        g.fillRoundedRectangle (r, 3.f);
        auto h = juce::jlimit (0.0f, 1.0f, v);
        auto fill = r.removeFromBottom (r.getHeight() * h);
        g.setColour (clipped ? theme::c(theme::danger) : theme::c(theme::accent));
        g.fillRoundedRectangle (fill, 3.f);
    }
    float levelL = 0.f, levelR = 0.f; bool clippedL=false, clippedR=false;
};