#pragma once

#include <juce_graphics/juce_graphics.h>

namespace enginelab
{
constexpr int grid      = 8;
constexpr int cols      = 120;
constexpr int rows      = 80;
constexpr int width     = cols * grid;   // 960
constexpr int height    = rows * grid;   // 640

inline juce::Rectangle<int> rect (int col, int row, int w, int h) noexcept
{
    return { col * grid, row * grid, w * grid, h * grid };
}

inline juce::Rectangle<int> header()       { return rect (4,  4, 112, 8); }
inline juce::Rectangle<int> ticker()       { return rect (4, 12, 112, 4); }
inline juce::Rectangle<int> cartography()  { return rect (6, 18, 64, 42); }
inline juce::Rectangle<int> rings()        { return rect (74, 18, 34, 16); }
inline juce::Rectangle<int> harmonics()    { return rect (74, 36, 34, 18); }
inline juce::Rectangle<int> driveColumn()  { return rect (10,62, 14,14); }
inline juce::Rectangle<int> focusColumn()  { return rect (40,62, 14,14); }
inline juce::Rectangle<int> contourColumn(){ return rect (70,62, 14,14); }
inline juce::Rectangle<int> presetBox()    { return rect (4, 74, 48, 4); }
inline juce::Rectangle<int> presetNav()    { return rect (54,74, 16, 4); }
inline juce::Rectangle<int> statusLight()  { return rect (74,74, 6, 4); }
inline juce::Rectangle<int> consoleArea()  { return rect (4, 78, 112,4); }
inline juce::Rectangle<int> footerMeter()  { return rect (96,74, 20,4); }
inline juce::Rectangle<int> signature()    { return rect (104,78, 12,2); }
}

