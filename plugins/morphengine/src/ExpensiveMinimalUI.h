// ExpensiveMinimalUI.h
// A compact JUCE UI kit for morphEngine plugin
// Style: "Expensive Minimalism" (gunmetal + amber), 400x300px layout
// Adapted from ChatGPT 5 Pro implementation for morphEngine integration

#pragma once
#if __has_include(<JuceHeader.h>)
#include <JuceHeader.h>
#else
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#endif

#include <algorithm>
#include "MorphEngineAudioProcessor.h"
#include "ResonanceLoom.h"

// ============================================================================
// Utility colour & style constants
// ============================================================================
namespace gm // gunmetal
{
    static inline juce::Colour gunmetal()    { return juce::Colour (0xFF2A2A2A); } // #2A2A2A
    static inline juce::Colour nearBlack()   { return juce::Colour (0xFF0F0F0F); }
    static inline juce::Colour darkGrid()    { return juce::Colour (0xFF1D1D1D); }
    static inline juce::Colour midGrid()     { return juce::Colour (0xFF3A3A3A); }
    static inline juce::Colour panelHi()     { return juce::Colour (0xFF3B3B3B); }
    static inline juce::Colour panelLo()     { return juce::Colour (0xFF111111); }
    static inline juce::Colour amber()       { return juce::Colour (0xFFFFB000); } // #FFB000
    static inline juce::Colour textDim()     { return juce::Colour (0xFFC7C7C7).withAlpha (0.85f); }
    static inline juce::Colour textFaint()   { return juce::Colour (0xFFC7C7C7).withAlpha (0.55f); }

    static inline juce::Font smallCaps (float px, juce::Font::FontStyleFlags style = juce::Font::plain)
    {
        // Simulated small caps: uppercase + slight tracking
        auto f = juce::Font (juce::Font::getDefaultSansSerifFontName(), px, style);
        f.setExtraKerningFactor (0.06f);
        return f;
    }

    // Simple inner-bevel panel painter (subtle 3D, recessed or raised)
    static inline void drawBevelPanel (juce::Graphics& g, juce::Rectangle<float> r,
                                       bool recessed = true, float radius = 6.0f, float depth = 2.0f)
    {
        r = r.reduced (0.5f);
        juce::Path p; p.addRoundedRectangle (r, radius);

        // Base fill (very dark)
        juce::Colour base = recessed ? nearBlack() : gunmetal().darker (0.7f);
        g.setColour (base);
        g.fillPath (p);

        // Inner gradient for recess/raise
        juce::Colour c1 = recessed ? panelLo() : panelHi();
        juce::Colour c2 = recessed ? panelHi() : panelLo();
        juce::ColourGradient grad (c1, r.getTopLeft(), c2, r.getBottomRight(), false);
        grad.addColour (0.5, base);
        g.setGradientFill (grad);
        g.fillPath (p);

        // Subtle inner shadow/highlight (simulate machined lip)
        g.saveState();
        auto stroke = 1.2f;
        juce::Path border; border.addRoundedRectangle (r.reduced (0.7f), radius - 1.0f);
        juce::Colour edgeHi = juce::Colours::white.withAlpha (recessed ? 0.06f : 0.08f);
        juce::Colour edgeLo = juce::Colours::black.withAlpha (recessed ? 0.35f : 0.25f);

        // Top-left highlight
        g.setColour (edgeHi);
        g.strokePath (border, juce::PathStrokeType (stroke));
        // Bottom-right shadow
        g.addTransform (juce::AffineTransform::translation (0.5f, 0.7f));
        g.setColour (edgeLo);
        g.strokePath (border, juce::PathStrokeType (stroke));
        g.restoreState();
    }

    // Brushed metal background generator (cached per size)
    static inline juce::Image makeBrushed (juce::Rectangle<int> bounds)
    {
        juce::Image img (juce::Image::ARGB, bounds.getWidth(), bounds.getHeight(), true);
        juce::Graphics g (img);
        g.fillAll (gunmetal());

        // Very subtle directional strokes
        juce::Random rng (0xBADC0DE);
        for (int y = 0; y < bounds.getHeight(); ++y)
        {
            float alpha = 0.015f + 0.01f * std::sin (y * 0.01f);
            g.setColour (juce::Colours::white.withAlpha (alpha));
            auto yOff = (y % 3 == 0) ? 1.0f : 0.0f;
            for (int x = 0; x < bounds.getWidth(); x += 6)
                g.fillRect (float (x), float (y + yOff), 4.0f, 1.0f);
        }

        // Fine noise
        juce::Image noise (juce::Image::SingleChannel, bounds.getWidth(), bounds.getHeight(), true);
        juce::Graphics gn (noise);
        for (int y = 0; y < bounds.getHeight(); ++y)
            for (int x = 0; x < bounds.getWidth(); ++x)
            {
                juce::uint8 v = (juce::uint8) juce::jlimit (0, 255, 120 + int (rng.nextFloat() * 16.0f));
                noise.setPixelAt (x, y, juce::Colour (v, v, v, juce::uint8(255)));
            }
        g.setTiledImageFill (noise, 0, 0, 0.025f); // extremely subtle
        g.fillAll();

        return img;
    }

    // Uppercasing helper for "small caps" look
    static inline juce::String caps (juce::String s)
    {
        return s.toUpperCase();
    }
}

// ============================================================================
// Look & Feel
// ============================================================================
class GunmetalLookAndFeel : public juce::LookAndFeel_V4
{
public:
    GunmetalLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, gm::gunmetal());
        setColour (juce::PopupMenu::backgroundColourId, gm::nearBlack());
        setColour (juce::Slider::thumbColourId, gm::amber());
        setColour (juce::Slider::trackColourId, gm::midGrid().darker (0.6f));
        setColour (juce::Slider::backgroundColourId, gm::nearBlack());
        setColour (juce::ComboBox::backgroundColourId, gm::nearBlack());
        setColour (juce::ComboBox::textColourId, gm::textDim());
        setColour (juce::ComboBox::arrowColourId, gm::amber());
        setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId, gm::textDim());
        setColour (juce::TextButton::buttonColourId, gm::nearBlack());
        setColour (juce::TextButton::textColourOnId, gm::amber());
        setColour (juce::TextButton::textColourOffId, gm::textDim());
    }

    // Typography
    juce::Font getLabelFont (juce::Label&) override { return gm::smallCaps (12.0f, juce::Font::bold); }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return gm::smallCaps (12.0f); }
    juce::Font getPopupMenuFont() override { return gm::smallCaps (12.0f); }

    // Labels in small caps
    void drawLabel (juce::Graphics& g, juce::Label& label) override
    {
        g.fillAll (juce::Colours::transparentBlack);
        auto r = label.getLocalBounds().toFloat();
        auto f = getLabelFont (label);
        g.setFont (f);
        g.setColour (label.findColour (juce::Label::textColourId));
        g.drawText (gm::caps (label.getText()), r.toNearestInt(), juce::Justification::centredLeft, true);
    }

    // Hardware-style horizontal slider with subtle bevel + center detent notch
    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           const juce::Slider::SliderStyle style, juce::Slider& s) override
    {
        juce::ignoreUnused (minSliderPos, maxSliderPos, style);
        auto r = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (2.0f);

        // Track
        auto trackH = juce::jmin (6.0f, r.getHeight() * 0.35f);
        auto track = r.withSizeKeepingCentre (r.getWidth() - 8.0f, trackH);
        gm::drawBevelPanel (g, track, true, 3.0f, 1.5f);

        // Center detent notch
        auto cx = track.getX() + track.getWidth() * 0.5f;
        g.setColour (juce::Colours::white.withAlpha (0.18f));
        g.fillRect (juce::Rectangle<float> (1.0f, track.getHeight()).withCentre ({ cx, track.getCentreY() }));
        g.setColour (juce::Colours::black.withAlpha (0.3f));
        g.drawLine (cx + 1.0f, track.getY() + 1.0f, cx + 1.0f, track.getBottom() - 1.0f, 1.0f);

        // Active fill (amber glass)
        auto left = track.withRight (sliderPos).expanded (0, 1.0f);
        juce::ColourGradient glow (gm::amber().withAlpha (0.5f), left.getCentre(),
                                   gm::amber().withAlpha (0.12f), left.getBottomLeft(), true);
        g.setGradientFill (glow);
        g.fillRoundedRectangle (left, 2.0f);

        // Thumb (machined block with shadow)
        auto thumbW = 18.0f, thumbH = juce::jmax (trackH + 8.0f, 16.0f);
        juce::Rectangle<float> thumb (sliderPos - thumbW * 0.5f, r.getCentreY() - thumbH * 0.5f, thumbW, thumbH);

        // Drop shadow
        juce::DropShadow (juce::Colours::black.withAlpha (0.6f), 6, {}).drawForRectangle (g, thumb.toNearestInt());

        juce::Path tp; tp.addRoundedRectangle (thumb, 3.0f);
        juce::Colour hi = gm::panelHi();
        juce::Colour lo = gm::panelLo();
        juce::ColourGradient tg (hi, thumb.getTopLeft(), lo, thumb.getBottomRight(), false); tg.addColour (0.5, hi.brighter (0.05f));
        g.setGradientFill (tg); g.fillPath (tp);
        g.setColour (juce::Colours::black.withAlpha (0.55f)); g.strokePath (tp, juce::PathStrokeType (1.2f));

        // Active outline when dragging
        if (s.isMouseOverOrDragging())
        {
            g.setColour (gm::amber().withAlpha (0.70f));
            g.strokePath (tp, juce::PathStrokeType (1.5f));
        }
    }

    // Minimal combo box w/ amber arrow and inner bevel
    void drawComboBox (juce::Graphics& g, int w, int h, bool,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox& box) override
    {
        juce::ignoreUnused (buttonX, buttonY, buttonW, buttonH);
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
        gm::drawBevelPanel (g, r, true, 4.0f, 2.0f);

        auto textR = r.reduced (8.0f);
        g.setFont (getComboBoxFont (box));
        g.setColour (box.findColour (juce::ComboBox::textColourId));
        g.drawText (gm::caps (box.getText()), textR.toNearestInt(), juce::Justification::centredLeft, true);

        // Amber arrow
        auto ar = r.removeFromRight (18.0f).reduced (4.0f);
        juce::Path arrow;
        arrow.addTriangle (ar.getCentreX() - 5.0f, ar.getY() + 5.5f,
                           ar.getCentreX() + 5.0f, ar.getY() + 5.5f,
                           ar.getCentreX(),          ar.getBottom() - 3.5f);
        g.setColour (gm::amber().withAlpha (0.9f));
        g.fillPath (arrow);
    }

    // Small popup-menu items
    void drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator,
                            bool isActive, bool isHighlighted, bool isTicked, bool, const juce::String& text,
                            const juce::String&, const juce::Drawable*, const juce::Colour*) override
    {
        if (isSeparator)
        {
            g.setColour (gm::midGrid().withAlpha (0.4f));
            g.fillRect (area.withTrimmedLeft (8).withTrimmedRight (8).removeFromTop (1));
            return;
        }
        auto r = area.reduced (6);
        if (isHighlighted) { g.setColour (gm::amber().withAlpha (0.08f)); g.fillRoundedRectangle (r.toFloat(), 3.0f); }

        g.setColour (isActive ? gm::textDim() : gm::textFaint());
        g.setFont (gm::smallCaps (12.0f));
        g.drawText (gm::caps (text), r, juce::Justification::centredLeft, true);

        if (isTicked)
        {
            g.setColour (gm::amber());
            g.fillEllipse (r.removeFromLeft (8).withSizeKeepingCentre (6, 6).toFloat());
        }
    }
};

// ============================================================================
// CenterDetentSlider: soft snap around a centre value (e.g., 0.5 for MIX)
// ============================================================================
class CenterDetentSlider : public juce::Slider
{
public:
    CenterDetentSlider (double centreValue = 0.5, double snapRadius = 0.02)
        : centre (centreValue), snap (snapRadius) { setSliderStyle (juce::Slider::LinearHorizontal); setTextBoxStyle (NoTextBox, false, 0, 0); }

    double snapValue (double attemptedValue, DragMode) override
    {
        return (std::abs (attemptedValue - centre) <= snap ? centre : attemptedValue);
    }

    void setCentre (double c) { centre = c; }
    void setSnapRadius (double r) { snap = r; }

private:
    double centre, snap;
};

// ============================================================================
// XYMorphPad: recessed pad w/ grid, amber dot, corner labels & momentum
// ============================================================================
class XYMorphPad : public juce::Component, private juce::Timer
{
public:
    XYMorphPad()
    {
        setRepaintsOnMouseActivity (true);
        startTimerHz (60);
    }

    // Normalised [0..1] position
    juce::Point<float> getPosition() const noexcept { return pos; }
    void setPosition (juce::Point<float> p, bool send = true)
    {
        pos = { juce::jlimit (0.0f, 1.0f, p.x), juce::jlimit (0.0f, 1.0f, p.y) };
        if (send && onPositionChanged) onPositionChanged (pos);
        repaint();
    }

    void setCornerLabels (const juce::String& topLeft, const juce::String& topRight,
                          const juce::String& bottomLeft, const juce::String& bottomRight)
    {
        labels[0] = topLeft; labels[1] = topRight; labels[2] = bottomLeft; labels[3] = bottomRight; repaint();
    }

    std::function<void (juce::Point<float>)> onPositionChanged;
    std::function<void()> onGestureStart;
    std::function<void()> onGestureEnd;
    bool momentumEnabled = true;

    void paint (juce::Graphics& g) override
    {
        g.addTransform (juce::AffineTransform()); // ensure AA state
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);

        auto r = getLocalBounds().toFloat();
        gm::drawBevelPanel (g, r, true, 8.0f, 2.0f);

        // Inner recess for pad surface
        auto inner = r.reduced (6.0f);
        juce::Path frame; frame.addRoundedRectangle (inner, 6.0f);
        g.setColour (gm::nearBlack());
        g.fillPath (frame);

        // Subtle grid
        drawGrid (g, inner);

        // Corner labels (small caps)
        g.setColour (gm::textFaint());
        g.setFont (gm::smallCaps (10.0f, juce::Font::bold));
        auto tl = inner.removeFromTop (16.0f); inner = getLocalBounds().toFloat().reduced (6.0f);
        g.drawText (gm::caps (labels[0]), tl.removeFromLeft (50.0f).toNearestInt(), juce::Justification::topLeft);
        g.drawText (gm::caps (labels[1]), juce::Rectangle<int> (getWidth() - 56, 6, 50, 12), juce::Justification::topRight);
        g.drawText (gm::caps (labels[2]), juce::Rectangle<int> (6, getHeight() - 18, 50, 12), juce::Justification::bottomLeft);
        g.drawText (gm::caps (labels[3]), juce::Rectangle<int> (getWidth() - 56, getHeight() - 18, 50, 12), juce::Justification::bottomRight);

        // Position dot (amber with glow + drop shadow)
        auto dot = toPixel (pos) .toFloat();
        float radius = 6.0f;

        // Soft glow
        for (int i = 0; i < 3; ++i)
        {
            float rr = radius + 6.0f + i * 3.0f;
            g.setColour (gm::amber().withAlpha (0.08f - i * 0.02f));
            g.fillEllipse (juce::Rectangle<float> (rr, rr).withCentre (dot));
        }

        // Drop shadow
        juce::DropShadow (juce::Colours::black.withAlpha (0.6f), 5, {}).drawForRectangle (g,
                juce::Rectangle<int> ((int) (dot.x - radius), (int) (dot.y - radius), (int) (radius*2), (int) (radius*2)));

        // Dot body (machined cap)
        juce::Path dp; dp.addEllipse (dot.x - radius, dot.y - radius, radius * 2.0f, radius * 2.0f);
        juce::ColourGradient grad (gm::amber().withAlpha (0.95f), dot.translated (-radius * 0.3f, -radius * 0.3f),
                                   gm::amber().darker (0.5f), dot.translated (radius * 0.7f, radius * 0.7f), true);
        g.setGradientFill (grad); g.fillPath (dp);
        g.setColour (juce::Colours::black.withAlpha (0.7f)); g.strokePath (dp, juce::PathStrokeType (1.0f));
    }

    void resized() override { cachedInner = getLocalBounds().reduced (6); }

    // Interaction
    void mouseDown (const juce::MouseEvent& e) override
    {
        inertia = {};
        dragLastTime = juce::Time::getMillisecondCounterHiRes();
        if (onGestureStart)
            onGestureStart();
        setWithEvent (e);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        auto now = juce::Time::getMillisecondCounterHiRes();
        double dt = juce::jmax (1.0, now - dragLastTime);
        juce::Point<float> old = pos;
        setWithEvent (e);

        // Velocity in normalized units / frame
        auto dp = pos - old;
        vel = (dp * (float) (16.0 / dt)); // ~approx 60 FPS basis
        dragLastTime = now;
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        inertia = momentumEnabled ? vel : juce::Point<float> {};
        if (onGestureEnd)
            onGestureEnd();
    }

private:
    juce::Point<float> pos { 0.5f, 0.5f };
    juce::Point<float> vel {};
    juce::Point<float> inertia {};
    double dragLastTime = 0.0;
    juce::Rectangle<int> cachedInner;
    juce::String labels[4] { "A", "B", "C", "D" };

    void timerCallback() override
    {
        if (inertia.getDistanceFromOrigin() > 1.0e-5f)
        {
            pos += inertia;
            // Friction & edge damping
            inertia *= 0.92f;

            // Bounce/clamp
            if (pos.x < 0.0f) { pos.x = 0.0f; inertia.x = -inertia.x * 0.45f; }
            if (pos.y < 0.0f) { pos.y = 0.0f; inertia.y = -inertia.y * 0.45f; }
            if (pos.x > 1.0f) { pos.x = 1.0f; inertia.x = -inertia.x * 0.45f; }
            if (pos.y > 1.0f) { pos.y = 1.0f; inertia.y = -inertia.y * 0.45f; }

            if (onPositionChanged) onPositionChanged (pos);
            repaint();
        }
    }

    // Helpers
    juce::Point<int> toPixel (juce::Point<float> np) const
    {
        auto inner = getLocalBounds().reduced (6);
        auto x = juce::roundToInt (inner.getX() + np.x * (float) inner.getWidth());
        auto y = juce::roundToInt (inner.getY() + np.y * (float) inner.getHeight());
        return { x, y };
    }

    void setWithEvent (const juce::MouseEvent& e)
    {
        auto inner = getLocalBounds().reduced (6);
        auto nx = juce::jlimit (0.0f, 1.0f, (float) (e.position.x - inner.getX()) / (float) inner.getWidth());
        auto ny = juce::jlimit (0.0f, 1.0f, (float) (e.position.y - inner.getY()) / (float) inner.getHeight());
        setPosition ({ nx, ny });
    }

    void drawGrid (juce::Graphics& g, juce::Rectangle<float> inner)
    {
        g.saveState();
        g.setColour (gm::darkGrid());
        // Draw small 5x5 grid
        const int nx = 5, ny = 5;
        for (int i = 1; i < nx; ++i)
        {
            float x = inner.getX() + inner.getWidth() * (float) i / (float) nx;
            g.drawLine (x, inner.getY() + 2.0f, x, inner.getBottom() - 2.0f, 1.0f);
        }
        for (int j = 1; j < ny; ++j)
        {
            float y = inner.getY() + inner.getHeight() * (float) j / (float) ny;
            g.drawLine (inner.getX() + 2.0f, y, inner.getRight() - 2.0f, y, 1.0f);
        }
        g.restoreState();
    }
};

// ============================================================================
// SpectrumDisplay: real-time FFT with grid and subtle peak glow
// ============================================================================
class SpectrumDisplay : public juce::Component, private juce::Timer
{
public:
    explicit SpectrumDisplay (int fftOrderToUse = 11 /* 2048 */)
        : order (fftOrderToUse),
          fftSize (1 << order),
          forwardFFT (order),
          window (fftSize, juce::dsp::WindowingFunction<float>::hann, true)
    {
        fifo.resize (fftSize * 4);
        fftData.resize ((size_t) fftSize * 2);
        magnitudes.resize (fftSize / 2);
        startTimerHz (60);
    }

    void pushSamples (const float* input, int numSamples)
    {
        // Call from the audio thread: push samples into FIFO (mono)
        for (int i = 0; i < numSamples; ++i)
            pushNextSampleIntoFifo (input[i]);
    }

    void clear()
    {
        std::fill (magnitudes.begin(), magnitudes.end(), 0.0f);
        std::fill (fifo.begin(), fifo.end(), 0.0f);
        std::fill (fftData.begin(), fftData.end(), 0.0f);
        fifoIndex = 0;
        nextFFTBlockReady.store (false);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        gm::drawBevelPanel (g, r, true, 6.0f, 2.0f);

        auto inner = r.reduced (6.0f);
        drawGrid (g, inner);

        // FFT path
        juce::Path path;
        path.preallocateSpace (8 * (int) inner.getWidth());
        const float minDb = -90.0f, maxDb = 0.0f;

        auto toY = [=](float mag)
        {
            auto db = juce::Decibels::gainToDecibels (mag);
            db = juce::jlimit (minDb, maxDb, db);
            return juce::jmap (db, minDb, maxDb, inner.getBottom(), inner.getY());
        };

        // Log-frequency X mapping
        auto toX = [=](int bin)
        {
            auto proportion = (float) bin / (float) (magnitudes.size() - 1);
            // Map 20 Hz .. Nyquist as log (visual only)
            float minHz = 20.0f, maxHz = 22050.0f;
            float hz = juce::jmap (proportion, 0.0f, 1.0f, minHz, maxHz);
            float logp = std::log10 (juce::jmax (1.001f, hz)) / std::log10 (maxHz);
            return inner.getX() + logp * inner.getWidth();
        };

        // Build line
        bool first = true;
        for (int i = 1; i < (int) magnitudes.size(); ++i)
        {
            auto x = toX (i);
            auto y = toY (magnitudes[(size_t) i]);
            if (first) { path.startNewSubPath (x, y); first = false; }
            else        path.lineTo (x, y);
        }

        // Amber path + subtle glow at peaks
        g.setColour (gm::amber().withAlpha (0.8f));
        g.strokePath (path, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Peak glow (local maxima above threshold)
        drawPeaks (g, inner, toX, toY);

        // Soft top/bottom mask vignette
        juce::ColourGradient vign (juce::Colours::black.withAlpha (0.15f), inner.getTopLeft(),
                                   juce::Colours::transparentBlack, inner.getBottomLeft(), false);
        g.setGradientFill (vign); g.fillRect (inner.removeFromTop (10.0f));
        g.setGradientFill (vign); g.fillRect (inner.removeFromBottom (10.0f));
    }

    void resized() override {}

private:
    const int order;
    const int fftSize;
    juce::dsp::FFT forwardFFT;
    juce::dsp::WindowingFunction<float> window;

    std::vector<float> fifo;
    std::vector<float> fftData;
    std::vector<float> magnitudes;
    int fifoIndex = 0;
    std::atomic<bool> nextFFTBlockReady { false };

    void timerCallback() override
    {
        if (nextFFTBlockReady.exchange (false))
        {
            // Apply window + perform FFT
            std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);
            window.multiplyWithWindowingTable (fftData.data(), fftSize);
            forwardFFT.performRealOnlyForwardTransform (fftData.data());

            // Magnitude (smoothed a touch)
            const float smooth = 0.65f;
            for (int i = 0; i < fftSize / 2; ++i)
            {
                auto re = fftData[(size_t) (2 * i)];
                auto im = fftData[(size_t) (2 * i + 1)];
                float mag = std::sqrt (re * re + im * im) / (float) fftSize;
                magnitudes[(size_t) i] = magnitudes[(size_t) i] * smooth + mag * (1.0f - smooth);
            }
            repaint();
        }
    }

    void pushNextSampleIntoFifo (float s)
    {
        if (fifoIndex == fftSize)
        {
            if (! nextFFTBlockReady.load())
            {
                // Copy into FFT input
                std::copy (fifo.begin(), fifo.begin() + fftSize, fftData.begin());
                nextFFTBlockReady.store (true);
            }
            fifoIndex = 0;
        }
        fifo[(size_t) fifoIndex++] = s;
    }

    void drawGrid (juce::Graphics& g, juce::Rectangle<float> inner)
    {
        g.saveState();
        g.setColour (gm::nearBlack());
        g.fillRoundedRectangle (inner, 5.0f);

        // Fine grid
        g.setColour (gm::midGrid().withAlpha (0.35f));
        // Vertical lines (log-ish positions for common frequencies)
        const float marks[] = { 31.5f, 63.0f, 125.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f };
        auto toX = [&](float hz)
        {
            float maxHz = 22050.0f;
            float logp = std::log10 (juce::jmax (1.001f, hz)) / std::log10 (maxHz);
            return inner.getX() + logp * inner.getWidth();
        };
        for (auto hz : marks)
        {
            float x = toX (hz);
            g.drawLine (x, inner.getY()+3, x, inner.getBottom()-3, 1.0f);
        }
        // Horizontal lines (dB)
        for (int db = -60; db <= 0; db += 20)
        {
            float y = juce::jmap ((float) db, -90.0f, 0.0f, inner.getBottom(), inner.getY());
            g.drawLine (inner.getX()+3, y, inner.getRight()-3, y, 1.0f);
        }
        g.restoreState();
    }

    template <typename ToX, typename ToY>
    void drawPeaks (juce::Graphics& g, juce::Rectangle<float> inner, ToX&& toX, ToY&& toY)
    {
        // Simple local-max peak finder
        std::vector<int> peaks;
        peaks.reserve (32);
        for (int i = 2; i < (int) magnitudes.size() - 2; ++i)
        {
            float m = magnitudes[(size_t) i];
            if (m > magnitudes[(size_t) i-1] && m > magnitudes[(size_t) i+1] && m > 0.005f)
                peaks.push_back (i);
        }

        // Glow per peak
        for (auto i : peaks)
        {
            auto p = juce::Point<float> (toX (i), toY (magnitudes[(size_t) i]));
            for (int ring = 0; ring < 3; ++ring)
            {
                float rr = 6.0f + ring * 4.0f;
                g.setColour (gm::amber().withAlpha (0.10f - ring * 0.03f));
                g.fillEllipse (juce::Rectangle<float> (rr, rr).withCentre (p));
            }
            // Small bright dot
            g.setColour (gm::amber());
            g.fillEllipse (juce::Rectangle<float> (2.0f, 2.0f).withCentre (p));
        }
    }
};

// ============================================================================
// MorphEngineExpensiveUI: 400x300 morphEngine UI adapted from ChatGPT template
// ============================================================================
class MorphEngineExpensiveUI : public juce::AudioProcessorEditor,
                                   private juce::Timer,
                                   private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit MorphEngineExpensiveUI(MorphEngineAudioProcessor& p)
        : AudioProcessorEditor(&p), processor(p), loom(p)
    {
        setSize (400, 300);
        setLookAndFeel (&lnf);

        processor.apvts.addParameterListener ("zplane.morph", this);
        processor.apvts.addParameterListener ("zplane.resonance", this);

        // XY Morph Pad - map to zplane.morph (X) and zplane.resonance (Y)
        xy.setCornerLabels ("WARM", "BRIGHT", "DARK", "AIR");
        xy.onGestureStart = [this]()
        {
            morphPadGestureActive = true;

            if (auto* morphParam = processor.apvts.getParameter ("zplane.morph"))
                morphParam->beginChangeGesture();
            if (auto* resonanceParam = processor.apvts.getParameter ("zplane.resonance"))
                resonanceParam->beginChangeGesture();
        };

        xy.onGestureEnd = [this]()
        {
            if (auto* morphParam = processor.apvts.getParameter ("zplane.morph"))
                morphParam->endChangeGesture();
            if (auto* resonanceParam = processor.apvts.getParameter ("zplane.resonance"))
                resonanceParam->endChangeGesture();

            morphPadGestureActive = false;
        };

        xy.onPositionChanged = [this](juce::Point<float> position)
        {
            if (auto* morphParam = processor.apvts.getParameter ("zplane.morph"))
            {
                auto& range = morphParam->getNormalisableRange();
                morphParam->setValueNotifyingHost (range.convertTo0to1 (position.x));
            }

            if (auto* resonanceParam = processor.apvts.getParameter ("zplane.resonance"))
            {
                auto& range = resonanceParam->getNormalisableRange();
                resonanceParam->setValueNotifyingHost (range.convertTo0to1 (position.y));
            }

            lastXY = position;
        };
        addAndMakeVisible (xy);

        // Spectrum Display - connect to processor's audio analysis
        addAndMakeVisible (spec);

        // Resonance Loom visualization for EMU filter modes
        addAndMakeVisible (loom);

        // Mix slider (center detent at 70% which is morphEngine default)
        mix.setCentre (0.5);
        mix.setSnapRadius (0.02);
        mix.setRange(0.0, 1.0, 0.001);
        addAndMakeVisible (mix);

        // Style presets mapped to morphEngine's 3 variants
        styleBox.addItem (gm::caps ("AIR"), 1);
        styleBox.addItem (gm::caps ("LIQUID"), 2);
        styleBox.addItem (gm::caps ("PUNCH"), 3);
        styleBox.setSelectedId (1, juce::dontSendNotification);
        addAndMakeVisible (styleBox);

        // Additional controls for morphEngine
        driveKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        driveKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 12);
        driveKnob.setRange(0.0, 12.0, 0.1);
        addAndMakeVisible(driveKnob);

        brightnessKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        brightnessKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 12);
        brightnessKnob.setRange(-6.0, 6.0, 0.1);
        addAndMakeVisible(brightnessKnob);

        hardnessKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        hardnessKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 12);
        hardnessKnob.setRange(0.0, 1.0, 0.001);
        addAndMakeVisible(hardnessKnob);

        // Labels
        addAndMakeVisible (mixLabel);
        mixLabel.setText ("MIX", juce::dontSendNotification);
        mixLabel.setInterceptsMouseClicks (false, false);

        addAndMakeVisible (styleLabel);
        styleLabel.setText ("STYLE", juce::dontSendNotification);
        styleLabel.setInterceptsMouseClicks (false, false);

        addAndMakeVisible (driveLabel);
        driveLabel.setText ("DRIVE", juce::dontSendNotification);
        driveLabel.setInterceptsMouseClicks (false, false);

        addAndMakeVisible (brightnessLabel);
        brightnessLabel.setText ("BRIGHT", juce::dontSendNotification);
        brightnessLabel.setInterceptsMouseClicks (false, false);

        addAndMakeVisible (hardnessLabel);
        hardnessLabel.setText ("HARD", juce::dontSendNotification);
        hardnessLabel.setInterceptsMouseClicks (false, false);

        // APVTS attachments
        mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.apvts, "style.mix", mix);
        driveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.apvts, "drive.db", driveKnob);
        brightnessAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.apvts, "tilt.brightness", brightnessKnob);
        hardnessAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.apvts, "hardness", hardnessKnob);
        styleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            processor.apvts, "style.variant", styleBox);
        trimAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.apvts, "output.trim", trimSlider);
        safeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            processor.apvts, "safe.mode", safeToggle);

        // Initialize XY pad position from parameters
        auto morphValue = processor.apvts.getRawParameterValue("zplane.morph")->load();
        auto resonanceValue = processor.apvts.getRawParameterValue("zplane.resonance")->load();
        lastXY = { morphValue, resonanceValue };
        xy.setPosition({morphValue, resonanceValue}, false);

        // Analyzer toggle (default off)
        analyzerToggle.setButtonText ("ANALYZER");
        analyzerToggle.setClickingTogglesState (true);
        analyzerToggle.onClick = [this]()
        {
            setAnalyzerEnabled (analyzerToggle.getToggleState());
        };
        addAndMakeVisible (analyzerToggle);

        // Safe mode toggle
        safeToggle.setButtonText ("SAFE MODE");
        safeToggle.setClickingTogglesState (true);
        addAndMakeVisible (safeToggle);

        // Output trim slider
        trimSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        trimSlider.setRange (-12.0, 12.0, 0.1);
        trimSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible (trimSlider);

        trimLabel.setText ("TRIM", juce::dontSendNotification);
        trimLabel.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (trimLabel);

        peakLabel.setJustificationType (juce::Justification::centredRight);
        peakLabel.setFont (gm::smallCaps (11.0f));
        peakLabel.setColour (juce::Label::textColourId, gm::textDim());
        peakLabel.setInterceptsMouseClicks (false, false);
        peakLabel.setText ("-inf dB", juce::dontSendNotification);
        addAndMakeVisible (peakLabel);

        clipLabel.setText ("CLIP", juce::dontSendNotification);
        clipLabel.setJustificationType (juce::Justification::centred);
        clipLabel.setFont (gm::smallCaps (10.0f, juce::Font::bold));
        clipLabel.setColour (juce::Label::textColourId, gm::textFaint());
        clipLabel.setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        clipLabel.setOpaque (true);
        clipLabel.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (clipLabel);

        startTimerHz (30);

        analyzerToggle.setToggleState (false, juce::dontSendNotification);
        setAnalyzerEnabled (false);
    }

    ~MorphEngineExpensiveUI() override
    {
        processor.apvts.removeParameterListener ("zplane.morph", this);
        processor.apvts.removeParameterListener ("zplane.resonance", this);

        setLookAndFeel (nullptr);
        stopTimer();
    }

    // Feed spectrum display with audio data and update meters
    void timerCallback() override
    {
        updateMeters();

        if (analyzerEnabled)
            updateSpectrumFromProcessor();
    }

    void updateSpectrumFromProcessor()
    {
        if (! processor.fillSpectrumSnapshot (spectrumScratch.data(), (int) spectrumScratch.size()))
            return;

        spec.pushSamples (spectrumScratch.data(), (int) spectrumScratch.size());
        spec.repaint();
    }

    void updateMeters()
    {
        const float peak = processor.getOutputPeak();
        const auto db = juce::Decibels::gainToDecibels (juce::jmax (peak, 1.0e-4f));

        auto peakString = (db > -90.0f) ? juce::String (db, 1) + " dB" : juce::String ("-inf dB");
        if (peakLabel.getText() != peakString)
            peakLabel.setText (peakString, juce::dontSendNotification);

        const bool clipActive = processor.isClipActive();
        clipLabel.setColour (juce::Label::textColourId, clipActive ? gm::amber() : gm::textFaint());
        clipLabel.setColour (juce::Label::backgroundColourId, clipActive ? gm::amber().withAlpha (0.35f) : juce::Colours::transparentBlack);
    }

    void setAnalyzerEnabled (bool enabled)
    {
        if (analyzerEnabled == enabled)
            return;

        analyzerEnabled = enabled;
        analyzerToggle.setToggleState (analyzerEnabled, juce::dontSendNotification);
        spec.setVisible (analyzerEnabled);
        loom.setVisible (! analyzerEnabled);

        if (analyzerEnabled)
            updateSpectrumFromProcessor();
        else
            spec.clear();
    }

    void parameterChanged (const juce::String& parameterID, float newValue) override
    {
        if (parameterID != "zplane.morph" && parameterID != "zplane.resonance")
            return;

        if (morphPadGestureActive)
            return;

        juce::Component::SafePointer<MorphEngineExpensiveUI> safe (this);
        juce::MessageManager::callAsync ([safe, parameterID, newValue]
        {
            if (safe != nullptr)
                safe->updatePadFromParameter (parameterID, newValue);
        });
    }

    void paint (juce::Graphics& g) override
    {
        // Brushed background
        if (bg.getWidth() != getWidth() || bg.getHeight() != getHeight())
            bg = gm::makeBrushed (getLocalBounds());
        g.drawImage (bg, getLocalBounds().toFloat());

        // Title (minimal, small caps, amber accent)
        g.setFont (gm::smallCaps (12.0f, juce::Font::bold));
        g.setColour (gm::amber().withAlpha (0.85f));
        g.drawText (gm::caps ("MorphEngine"), juce::Rectangle<int> (10, 6, 160, 14), juce::Justification::topLeft);

        g.setColour (gm::textFaint());
        g.setFont (gm::smallCaps (11.0f));
        g.drawText (gm::caps ("Expensive Minimalism"), juce::Rectangle<int> (10, 22, 180, 12), juce::Justification::topLeft);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (14);

        auto top = r.removeFromTop (170);
        xy.setBounds (top.removeFromLeft (170).withTrimmedRight (20).withCentre (juce::Point<int> (top.getX() + 85, top.getY() + 85)).withSize (150, 150));
        auto responseArea = top.withTrimmedLeft (10);
        spec.setBounds (responseArea);
        loom.setBounds (responseArea);

        auto toggleRow = r.removeFromTop (30);
        analyzerToggle.setBounds (toggleRow.removeFromLeft (120).reduced (0, 4));
        safeToggle.setBounds (toggleRow.removeFromLeft (120).reduced (0, 4));

        auto mid = r.removeFromTop (48);
        styleLabel.setBounds (mid.removeFromLeft (50));
        styleBox.setBounds (mid.removeFromLeft (130).reduced (0, 4));

        auto knobArea = mid.withTrimmedLeft (16);
        auto knobWidth = knobArea.getWidth() / 3;

        auto driveArea = knobArea.removeFromLeft (knobWidth);
        driveLabel.setBounds (driveArea.removeFromTop (15));
        driveKnob.setBounds (driveArea.reduced (2));

        auto brightArea = knobArea.removeFromLeft (knobWidth);
        brightnessLabel.setBounds (brightArea.removeFromTop (15));
        brightnessKnob.setBounds (brightArea.reduced (2));

        auto hardArea = knobArea;
        hardnessLabel.setBounds (hardArea.removeFromTop (15));
        hardnessKnob.setBounds (hardArea.reduced (2));

        auto sliders = r.removeFromBottom (84);
        auto mixArea = sliders.removeFromTop (38);
        mixLabel.setBounds (mixArea.removeFromLeft (44));
        mix.setBounds (mixArea.reduced (0, 8));

        auto trimArea = sliders.removeFromTop (38);
        trimLabel.setBounds (trimArea.removeFromLeft (44));
        auto meterArea = trimArea.removeFromRight (78);
        trimSlider.setBounds (trimArea.reduced (0, 8));
        clipLabel.setBounds (meterArea.removeFromTop (18).reduced (4, 2));
        peakLabel.setBounds (meterArea.reduced (4, 2));
    }

private:
    MorphEngineAudioProcessor& processor;
    GunmetalLookAndFeel lnf;
    juce::Image bg;

    XYMorphPad xy;
    SpectrumDisplay spec;
    ResonanceLoom loom;
    CenterDetentSlider mix;
    juce::ComboBox styleBox;
    juce::Slider driveKnob, brightnessKnob, hardnessKnob;
    juce::ToggleButton analyzerToggle, safeToggle;
    juce::Slider trimSlider;

    juce::Label mixLabel, styleLabel, driveLabel, brightnessLabel, hardnessLabel, trimLabel, peakLabel, clipLabel;

    // APVTS attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> driveAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> brightnessAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> hardnessAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> styleAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> trimAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> safeAttachment;

    void updatePadFromParameter (const juce::String& parameterID, float value)
    {
        auto updated = lastXY;

        if (parameterID == "zplane.morph")
            updated.x = juce::jlimit (0.0f, 1.0f, value);
        else if (parameterID == "zplane.resonance")
            updated.y = juce::jlimit (0.0f, 1.0f, value);

        lastXY = updated;
        xy.setPosition (updated, false);
    }

    // State tracking
    juce::Point<float> lastXY { 0.5f, 0.5f };
    bool morphPadGestureActive = false;
    bool analyzerEnabled = true;
    std::array<float, 2048> spectrumScratch {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MorphEngineExpensiveUI)
};
