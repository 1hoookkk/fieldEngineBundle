#include "DevBindings.h"

namespace fe { namespace morphengine {

#if FE_HAVE_JUCE
bool FilterBrowserKeyHandler::keyPressed(const juce::KeyPress& key, juce::Component*) {
  // Right or ']' goes next; Left or '[' goes prev.
  if (key.getKeyCode() == juce::KeyPress::rightKey || key.getTextCharacter() == ']') {
    return browser.next(bridge, cfg);
  }
  if (key.getKeyCode() == juce::KeyPress::leftKey || key.getTextCharacter() == '[') {
    return browser.prev(bridge, cfg);
  }
  // Up/Down to jump by 5
  if (key.getKeyCode() == juce::KeyPress::upKey) {
    return browser.setIndex(browser.index() + 5, bridge, cfg);
  }
  if (key.getKeyCode() == juce::KeyPress::downKey) {
    return browser.setIndex(browser.index() - 5, bridge, cfg);
  }
  return false;
}
#endif

bool handleBrowserMidi(FilterBrowser& browser, pe::DspBridge& bridge, const fe::morphengine::EmuMapConfig& cfg, int midiNoteNumber, float /*velocity*/) noexcept {
  // C0 (12) = prev, C#0 (13) = next by convention during dev
  if (midiNoteNumber == 12) {
    return browser.prev(bridge, cfg);
  }
  if (midiNoteNumber == 13) {
    return browser.next(bridge, cfg);
  }
  return false;
}

}} // namespace fe::morphengine

