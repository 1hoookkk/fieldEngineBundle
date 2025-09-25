#pragma once

#if __has_include(<juce_gui_extra/juce_gui_extra.h>)
  #include <juce_gui_extra/juce_gui_extra.h>
  #define FE_HAVE_JUCE 1
#else
  #define FE_HAVE_JUCE 0
#endif

#include "FilterBrowser.h"
#include <zplane_engine/DspBridge.h>
#include <zplane_models/EmuMapConfig.h>

namespace fe { namespace morphengine {

#if FE_HAVE_JUCE
class FilterBrowserKeyHandler : public juce::KeyListener
{
public:
    FilterBrowserKeyHandler(FilterBrowser& browserIn,
                            pe::DspBridge& bridgeIn,
                            const fe::morphengine::EmuMapConfig& configIn) noexcept
        : browser(browserIn), bridge(bridgeIn), cfg(configIn) {}

    bool keyPressed(const juce::KeyPress& key, juce::Component*) override;

    void attachTo(juce::Component& comp) { comp.addKeyListener(this); }
    void detachFrom(juce::Component& comp) { comp.removeKeyListener(this); }

private:
    FilterBrowser& browser;
    pe::DspBridge& bridge;
    fe::morphengine::EmuMapConfig cfg {};
};
#endif

bool handleBrowserMidi(FilterBrowser& browser,
                       pe::DspBridge& bridge,
                       const fe::morphengine::EmuMapConfig& cfg,
                       int midiNoteNumber,
                       float velocity) noexcept;

}} // namespace fe::morphengine
