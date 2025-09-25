#include "CartographyView.h"

CartographyView::CartographyView() {
    history.assign(2048, 0.f);
    startTimerHz(60);
}

void CartographyView::setMode(Mode m) { mode = m; repaint(); }

void CartographyView::setDriveDb(float dB)   { driveDb = dB;   lastChangeMs = juce::Time::getMillisecondCounter(); }
void CartographyView::setFocus01(float f)    { focus01 = juce::jlimit(0.f,1.f,f); lastChangeMs = juce::Time::getMillisecondCounter(); }
void CartographyView::setContour(float c)    { contour = juce::jlimit(-1.f,1.f,c); lastChangeMs = juce::Time::getMillisecondCounter(); }

void CartographyView::setTelemetry(const fe::MorphEngine::Telemetry& t) { lastTel = t; }

void CartographyView::pushMonoSamples(std::span<const float> mono) {
    for (float s : mono) { history[idx] = s; idx = (idx + 1) % history.size(); }
}

void CartographyView::paint(juce::Graphics& g) {
    auto* laf = dynamic_cast<FELookAndFeel*>(&getLookAndFeel());
    auto area = getLocalBounds().reduced(8);

    if (laf != nullptr) {
        laf->drawPixelBorder(g, getLocalBounds(), 1);
        laf->drawGrid8(g, area);
    }

    switch (mode) {
        case Waveform:   drawWaveform(g, area); break;
        case Spectrum:   drawSpectrum(g, area); break;
        case VectorField:drawVector(g, area); break;
    }

    // Crosshair + readout
    g.setColour(juce::Colours::white);
    auto c = area.getCentre();
    g.drawLine((float)c.x - 8, (float)c.y, (float)c.x + 8, (float)c.y, 1.0f);
    g.drawLine((float)c.x, (float)c.y - 8, (float)c.x, (float)c.y + 8, 1.0f);

    juce::Rectangle<int> badge = area.removeFromBottom(20).removeFromRight(180);
    if (laf != nullptr) g.setColour(laf->BORDER); else g.setColour(juce::Colours::black);
    g.fillRect(badge);
    g.setColour(juce::Colours::white);
    g.setFont(12.0f);
    juce::String txt = juce::String::formatted("FREQ: %.0fHZ  GAIN: %+0.2fDB",
                    200.0 + lastTel.morphX * 7800.0, (double)driveDb);
    g.drawFittedText(txt, badge.reduced(4), juce::Justification::left, 1);

    if (lastTel.clipped) {
        auto r = getLocalBounds().removeFromBottom(18).removeFromRight(64);
        if (laf != nullptr) g.setColour(laf->ERRORC); else g.setColour(juce::Colours::red);
        g.drawText("[CLIP]", r, juce::Justification::centredRight);
    }
}

void CartographyView::drawWaveform(juce::Graphics& g, juce::Rectangle<int> r) {
    auto* laf = dynamic_cast<FELookAndFeel*>(&getLookAndFeel());
    // ghost trace
    const auto msSince = juce::Time::getMillisecondCounter() - lastChangeMs;
    if (msSince < 2000) {
        g.setColour((laf ? laf->TEXT : juce::Colours::white).withAlpha(0.25f));
        juce::Path ghost;
        const int N = (int)history.size();
        for (int i=0;i<N;++i) {
            const int xi = r.getX() + (i * r.getWidth()) / N;
            const float s = history[(idx + i) % N] * 0.4f;
            const int yi = r.getCentreY() - (int)(s * (r.getHeight()/2));
            if (i==0) ghost.startNewSubPath((float)xi, (float)yi);
            else ghost.lineTo((float)xi, (float)yi);
        }
        g.strokePath(ghost, juce::PathStrokeType(1.0f));
    }

    // active stroke maps to Drive
    const float thick = juce::jmap(driveDb, 0.0f, 12.0f, 1.0f, 3.0f);
    g.setColour((laf ? laf->ACCENT : juce::Colours::white));
    juce::Path p;
    const int N = (int)history.size();
    for (int i=0;i<N;++i) {
        const int xi = r.getX() + (i * r.getWidth()) / N;
        float s = history[(idx + i) % N];
        // Contour tilt
        const float t = juce::jmap((float)i, 0.f, (float)N, -1.f, 1.f);
        s += contour * 0.15f * t;
        // Focus (center compression visual): squash y near center
        float local = std::abs((2.0f * i / (float)N) - 1.0f);
        const float compress = 1.0f - 0.4f * std::pow(1.0f - focus01, 1.5f) * (1.0f - local);
        const int yi = r.getCentreY() - (int)(s * compress * (r.getHeight()/2 - 4));
        if (i==0) p.startNewSubPath((float)xi, (float)yi);
        else p.lineTo((float)xi, (float)yi);
    }
    g.strokePath(p, juce::PathStrokeType(thick));
}

void CartographyView::drawSpectrum(juce::Graphics& g, juce::Rectangle<int> r) {
    auto* laf = dynamic_cast<FELookAndFeel*>(&getLookAndFeel());
    g.setColour(laf ? laf->ACCENT : juce::Colours::white);
    const int bars = 96;
    for (int i=0;i<bars;++i) {
        int x = r.getX() + (i * r.getWidth()) / bars;
        int h = 4 + ((i * 13) % (r.getHeight()-8)); // placeholder noise
        g.fillRect(x, r.getBottom()-h, 2, h);
    }
}

void CartographyView::drawVector(juce::Graphics& g, juce::Rectangle<int> r) {
    auto* laf = dynamic_cast<FELookAndFeel*>(&getLookAndFeel());
    g.setColour(laf ? laf->ACCENT : juce::Colours::white);
    const int cols = r.getWidth() / 16;
    const int rows = r.getHeight() / 16;
    for (int y=0; y<rows; ++y) for (int x=0; x<cols; ++x) {
        auto cx = r.getX() + x*16 + 8;
        auto cy = r.getY() + y*16 + 8;
        const float dx = (x - cols/2) * 0.03f * (0.6f + focus01);
        const float dy = (y - rows/2) * 0.03f * contour;
        g.drawLine((float)cx, (float)cy, (float)(cx + dx*12), (float)(cy + dy*12), 1.0f);
    }
}
