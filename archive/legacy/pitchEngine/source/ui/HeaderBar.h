#pragma once
#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"

// Simple header: title + chips (A/B, Bypass, Track/Print with latency, Secret).
class HeaderBar : public juce::Component {
public:
    std::function<void()> onAToggle, onBToggle, onBypassToggle, onQualityToggle, onSecretToggle;
    void setLatencyText (juce::String s) { latency = std::move(s); repaint(); }
    void setStates (bool a, bool b, bool byp, bool print, bool secret) { stA=a; stB=b; stByp=byp; stPrint=print; stSecret=secret; repaint(); }

    void paint (juce::Graphics& g) override {
        auto r = getLocalBounds().toFloat();
        g.setColour (theme::c(theme::bg));
        g.fillRect (r);
        g.setColour (theme::c(theme::text));
        g.setFont (juce::Font (16.f, juce::Font::bold));
        g.drawText ("pitchEngine Pro", r.removeFromLeft (200).toNearestInt(), juce::Justification::centredLeft);

        hotspots.clear();

        auto right = getLocalBounds().withTrimmedLeft (240).toFloat();
        auto cell  = [&](float w){ auto c = right.removeFromLeft (w); right.removeFromLeft (8); return c; };

        drawChip (g, cell(64),  "A",      stA,     onAToggle);
        drawChip (g, cell(64),  "B",      stB,     onBToggle);
        drawChip (g, cell(88),  "Bypass", stByp,   onBypassToggle);
        drawChip (g, cell(110), stPrint? "PRINT" : "TRACK", stPrint, onQualityToggle);
        drawChip (g, cell(92),  "Secret", stSecret,onSecretToggle);

        // Latency badge
        auto lb = right.removeFromLeft (110).reduced (2.f);
        g.setColour (theme::c(theme::border));
        g.drawRoundedRectangle (lb, 10.f, 1.f);
        g.setColour (theme::c(theme::muted));
        g.setFont (12.f);
        g.drawFittedText (latency, lb.toNearestInt(), juce::Justification::centred, 1);
    }

    void mouseUp (const juce::MouseEvent& e) override {
        for (auto& h : hotspots) if (h.r.contains (e.getPosition())) { if (h.cb) h.cb(); break; }
    }

private:
    struct Hot { juce::Rectangle<int> r; std::function<void()> cb; };
    juce::Array<Hot> hotspots;

    void drawChip (juce::Graphics& g, juce::Rectangle<float> box, juce::String txt, bool on, std::function<void()> cb) {
        auto rr = box.reduced (2.f);
        auto c = on ? theme::c(theme::accent) : theme::c(theme::border);
        g.setColour (theme::c(theme::panel));
        g.fillRoundedRectangle (rr, 12.f);
        g.setColour (c);
        g.drawRoundedRectangle (rr, 12.f, 1.5f);
        g.setColour (on ? theme::c(theme::text) : theme::c(theme::muted));
        g.setFont (14.f);
        g.drawFittedText (txt, rr.toNearestInt(), juce::Justification::centred, 1);
        hotspots.add ({ rr.toNearestInt(), std::move (cb) });
    }

    juce::String latency = "≤5 ms";
    bool stA=false, stB=false, stByp=false, stPrint=false, stSecret=false;
};