#include "FELookAndFeel.h"
#include <BinaryData.h>

FELookAndFeel::FELookAndFeel() {
    setColour (juce::ResizableWindow::backgroundColourId, BG);

    // Try to load an embedded pixel font if present; otherwise fall back to system monospace
    int fontDataSize = 0;
    const void* fontData = nullptr;
    if (auto* ptr = BinaryData::getNamedResource("metasynth_ttf", fontDataSize)) {
        fontData = ptr;
    } else if (auto* ptr2 = BinaryData::getNamedResource("metasynth.ttf", fontDataSize)) {
        fontData = ptr2;
    }

    if (fontData != nullptr && fontDataSize > 0) {
        pixelTypeface = juce::Typeface::createSystemTypefaceFor(fontData, fontDataSize);
        setDefaultSansSerifTypeface(pixelTypeface);
    }
}

void FELookAndFeel::drawPixelBorder(juce::Graphics& g, juce::Rectangle<int> r, int px) const {
    g.setColour(BORDER); g.drawRect(r, px);
}

void FELookAndFeel::drawGrid8(juce::Graphics& g, juce::Rectangle<int> r) const {
    g.setColour(BORDER.withAlpha(0.2f));
    for (int x=r.getX(); x<r.getRight(); x+=grid) g.drawLine((float)x, (float)r.getY(), (float)x, (float)r.getBottom());
    for (int y=r.getY(); y<r.getBottom(); y+=grid) g.drawLine((float)r.getX(), (float)y, (float)r.getRight(), (float)y);
}

void FELookAndFeel::titleBar(juce::Graphics& g, juce::Rectangle<int> r,
                             const juce::String& left, const juce::String& center, const juce::String& right) const {
    g.setColour(BORDER); g.fillRect(r);
    g.setColour(juce::Colours::white);
    juce::Font f = pixelTypeface != nullptr ? juce::Font(pixelTypeface).withHeight(12.0f) : juce::Font(12.0f);
    g.setFont(f);
    g.drawText(left,   r.removeFromLeft(r.getWidth()/3), juce::Justification::left, false);
    g.setColour(ACCENT);
    g.drawText(center, r.removeFromLeft(r.getWidth()/2), juce::Justification::centred, false);
    g.drawText(right,  r, juce::Justification::right, false);
}
