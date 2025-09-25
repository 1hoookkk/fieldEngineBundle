#pragma once
#include <juce_graphics/juce_graphics.h>

// Brand tokens
namespace theme {
    static constexpr auto bg      = 0xFF0B0F14; // window bg
    static constexpr auto panel   = 0xFF121823;
    static constexpr auto border  = 0xFF1C2330;
    static constexpr auto text    = 0xFFE6EEF9;
    static constexpr auto muted   = 0xFF98A6BD;
    static constexpr auto accent  = 0xFF67E0C1;
    static constexpr auto danger  = 0xFFFF6A6A;

    static inline juce::Colour c (uint32_t argb) { return juce::Colour (argb); }
}