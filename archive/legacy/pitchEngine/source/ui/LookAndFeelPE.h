#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include "Theme.h"

// Minimal premium look: rotary arc, hover glow, value box below.
class LookAndFeelPE : public juce::LookAndFeel_V4 {
public:
    LookAndFeelPE() { setColourDefaults(); }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& s) override
    {
        const auto r   = juce::Rectangle<float>(x, y, w, h).reduced(8.0f);
        const auto ctr = r.getCentre();
        const float radius = juce::jmin(r.getWidth(), r.getHeight()) * 0.5f;
        const float ang    = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        // Back plate
        g.setColour(theme::c(theme::panel));
        g.fillEllipse(r);

        // Inner shadow ring
        g.setColour(theme::c(theme::border));
        g.drawEllipse(r, 1.5f);

        // Active arc
        juce::Path arc;
        arc.addCentredArc(ctr.x, ctr.y, radius - 6.f, radius - 6.f, 0.f, rotaryStartAngle, ang, true);
        g.setColour(theme::c(theme::accent).withAlpha(0.95f));
        g.strokePath(arc, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Tick
        const float tickLen = radius * 0.75f;
        juce::Point<float> p (ctr.x + tickLen * std::cos(ang), ctr.y + tickLen * std::sin(ang));
        g.setColour(theme::c(theme::text));
        g.drawLine(ctr.x, ctr.y, p.x, p.y, 1.4f); // explicit overload

        // Hover glow
        if (s.isMouseOverOrDragging())
            g.setColour(theme::c(theme::accent).withAlpha(0.08f)),
            g.fillEllipse(r.expanded(2.f));

        // Label (below)
        auto name = s.getName();
        g.setFont(juce::Font(12.0f, juce::Font::plain));
        g.setColour(theme::c(theme::muted));
        g.drawFittedText(name, juce::Rectangle<int>(x, y + h - 22, w, 18), juce::Justification::centred, 1);
    }

    juce::Label* createSliderTextBox(juce::Slider& s) override {
        auto* l = new juce::Label();
        l->setJustificationType(juce::Justification::centred);
        l->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        l->setColour(juce::Label::textColourId, theme::c(theme::text));
        l->setFont(juce::Font(12.0f));
        return l;
    }

    void drawComboBox(juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override {
        juce::Rectangle<int> r(0,0,w,h);
        g.setColour(theme::c(theme::panel));
        g.fillRoundedRectangle(r.toFloat(), 6.f);
        g.setColour(theme::c(theme::border));
        g.drawRoundedRectangle(r.toFloat().reduced(0.5f), 6.f, 1.0f);
        g.setColour(theme::c(theme::text));
        g.setFont(14.0f);
        g.drawFittedText(box.getText(), r.reduced(8, 2), juce::Justification::centredLeft, 1);
    }

    void setColourDefaults() {
        setColour(juce::ResizableWindow::backgroundColourId, theme::c(theme::bg));
        setColour(juce::Slider::textBoxTextColourId, theme::c(theme::text));
        setColour(juce::PopupMenu::textColourId, theme::c(theme::text));
        setColour(juce::PopupMenu::backgroundColourId, theme::c(theme::panel));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, theme::c(theme::border));
    }
};