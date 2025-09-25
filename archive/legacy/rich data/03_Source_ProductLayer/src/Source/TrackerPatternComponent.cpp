/******************************************************************************
 * File: TrackerPatternComponent.cpp
 * Description: Revolutionary FastTracker2/ProTracker-style pattern editor implementation
 * 
 * REVOLUTIONARY TRACKER INTERFACE IMPLEMENTATION
 * Authentic hexadecimal pattern editing with chunky pixelated display
 * 
 * Copyright (c) 2025 Spectral Audio Systems
 ******************************************************************************/

#include "TrackerPatternComponent.h"
#include <cmath>

//==============================================================================
// Constructor - Initialize Revolutionary Tracker Interface
//==============================================================================

TrackerPatternComponent::TrackerPatternComponent()
{
    // Initialize all patterns
    for (int i = 0; i < MAX_PATTERNS; ++i)
    {
        patterns[i].clear();
        patterns[i].patternName = "PATTERN " + juce::String(i).paddedLeft('0', 2);
    }
    
    // Set up tracker key listening
    setWantsKeyboardFocus(true);
    addKeyListener(this);
    
    // Start cursor blink timer
    startTimer(500); // 500ms blink rate like real trackers
}

TrackerPatternComponent::~TrackerPatternComponent()
{
    removeKeyListener(this);
}

//==============================================================================
// Revolutionary Visual Rendering
//==============================================================================

void TrackerPatternComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    
    // Draw brutal black background
    drawTrackerBackground(g);
    
    // Draw grid lines first (behind everything)
    drawGridLines(g);
    
    // Draw selection highlighting
    if (hasSelection)
        drawSelection(g);
    
    // Draw main tracker interface elements
    drawChannelHeaders(g);
    drawRowNumbers(g);
    drawPatternData(g);
    
    // Draw playback position if playing
    if (isPlaying && playbackRow >= 0)
        drawPlaybackPosition(g);
    
    // Draw cursor on top
    if (cursorVisible)
        drawCursor(g);
    
    // Draw status bar at bottom
    drawStatusBar(g);
}

void TrackerPatternComponent::drawTrackerBackground(juce::Graphics& g)
{
    // Pure black background like authentic trackers
    g.fillAll(juce::Colour(TrackerColors::backgroundBlack));
}

void TrackerPatternComponent::drawChannelHeaders(juce::Graphics& g)
{
    g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain));
    g.setColour(juce::Colour(TrackerColors::channelHeaders));
    
    int headerY = 5;
    int startX = 40; // After row numbers
    
    for (int ch = 0; ch < channelCount; ++ch)
    {
        int channelX = startX + ch * (CHANNEL_WIDTH * CHAR_WIDTH);
        
        // Channel number (like "CHN 01")
        juce::String channelText = "CH" + juce::String(ch + 1).paddedLeft('0', 2);
        g.drawText(channelText, channelX, headerY, CHANNEL_WIDTH * CHAR_WIDTH, CHAR_HEIGHT,
                   juce::Justification::centred, false);
        
        // Column headers (NOTE INS VOL EFX)
        int colX = channelX;
        g.setColour(juce::Colour(TrackerColors::textNote));
        g.drawText("NOT", colX, headerY + CHAR_HEIGHT, NOTE_WIDTH * CHAR_WIDTH, CHAR_HEIGHT,
                   juce::Justification::centred, false);
        colX += NOTE_WIDTH * CHAR_WIDTH + COLUMN_SPACING;
        
        g.setColour(juce::Colour(TrackerColors::textInstrument));
        g.drawText("IN", colX, headerY + CHAR_HEIGHT, INSTRUMENT_WIDTH * CHAR_WIDTH, CHAR_HEIGHT,
                   juce::Justification::centred, false);
        colX += INSTRUMENT_WIDTH * CHAR_WIDTH + COLUMN_SPACING;
        
        g.setColour(juce::Colour(TrackerColors::textVolume));
        g.drawText("VL", colX, headerY + CHAR_HEIGHT, VOLUME_WIDTH * CHAR_WIDTH, CHAR_HEIGHT,
                   juce::Justification::centred, false);
        colX += VOLUME_WIDTH * CHAR_WIDTH + COLUMN_SPACING;
        
        g.setColour(juce::Colour(TrackerColors::textEffect));
        g.drawText("E", colX, headerY + CHAR_HEIGHT, EFFECT_WIDTH * CHAR_WIDTH, CHAR_HEIGHT,
                   juce::Justification::centred, false);
        colX += EFFECT_WIDTH * CHAR_WIDTH + COLUMN_SPACING;
        
        g.drawText("PR", colX, headerY + CHAR_HEIGHT, PARAM_WIDTH * CHAR_WIDTH, CHAR_HEIGHT,
                   juce::Justification::centred, false);
    }
}

void TrackerPatternComponent::drawRowNumbers(juce::Graphics& g)
{
    g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain));
    g.setColour(juce::Colour(TrackerColors::rowNumbers));
    
    int startY = 35; // After headers
    
    for (int row = 0; row < TrackerPattern::PATTERN_LENGTH; ++row)
    {
        int rowY = startY + row * ROW_HEIGHT;
        
        // Draw row number in hex (00-3F)
        juce::String rowText = juce::String::toHexString(row).toUpperCase().paddedLeft('0', 2);
        g.drawText(rowText, 5, rowY, 30, ROW_HEIGHT, juce::Justification::centredLeft, false);
    }
}

void TrackerPatternComponent::drawPatternData(juce::Graphics& g)
{
    g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain));
    
    int startY = 35; // After headers
    int startX = 40; // After row numbers
    
    const TrackerPattern& pattern = getCurrentPattern();
    
    for (int row = 0; row < TrackerPattern::PATTERN_LENGTH; ++row)
    {
        int rowY = startY + row * ROW_HEIGHT;
        
        for (int ch = 0; ch < channelCount; ++ch)
        {
            int channelX = startX + ch * (CHANNEL_WIDTH * CHAR_WIDTH);
            const TrackerNote& note = pattern.notes[ch][row];
            
            int colX = channelX;
            
            // Draw note (C-4, D#5, etc)
            g.setColour(juce::Colour(TrackerColors::textNote));
            juce::String noteText = noteToString(note.note);
            g.drawText(noteText, colX, rowY, NOTE_WIDTH * CHAR_WIDTH, ROW_HEIGHT,
                       juce::Justification::centredLeft, false);
            colX += NOTE_WIDTH * CHAR_WIDTH + COLUMN_SPACING;
            
            // Draw instrument (01-99)
            g.setColour(juce::Colour(TrackerColors::textInstrument));
            juce::String instText = instrumentToString(note.instrument);
            g.drawText(instText, colX, rowY, INSTRUMENT_WIDTH * CHAR_WIDTH, ROW_HEIGHT,
                       juce::Justification::centredLeft, false);
            colX += INSTRUMENT_WIDTH * CHAR_WIDTH + COLUMN_SPACING;
            
            // Draw volume (00-64)
            g.setColour(juce::Colour(TrackerColors::textVolume));
            juce::String volText = volumeToString(note.volume);
            g.drawText(volText, colX, rowY, VOLUME_WIDTH * CHAR_WIDTH, ROW_HEIGHT,
                       juce::Justification::centredLeft, false);
            colX += VOLUME_WIDTH * CHAR_WIDTH + COLUMN_SPACING;
            
            // Draw effect (A-Z)
            g.setColour(juce::Colour(TrackerColors::textEffect));
            juce::String effText = effectToString(note.effect);
            g.drawText(effText, colX, rowY, EFFECT_WIDTH * CHAR_WIDTH, ROW_HEIGHT,
                       juce::Justification::centredLeft, false);
            colX += EFFECT_WIDTH * CHAR_WIDTH + COLUMN_SPACING;
            
            // Draw parameter (00-FF)
            g.setColour(juce::Colour(TrackerColors::textEffect)); // Same color as effect
            juce::String paramText = paramToString(note.effectParam);
            g.drawText(paramText, colX, rowY, PARAM_WIDTH * CHAR_WIDTH, ROW_HEIGHT,
                       juce::Justification::centredLeft, false);
        }
    }
}

void TrackerPatternComponent::drawCursor(juce::Graphics& g)
{
    // Draw blue cursor highlight like FastTracker2
    g.setColour(juce::Colour(TrackerColors::cursorHighlight));
    
    auto cellBounds = getCellBounds(cursorRow, cursorChannel, cursorColumn);
    g.fillRect(cellBounds);
    
    // Draw cursor border for extra visibility
    g.setColour(juce::Colour(TrackerColors::textDefault));
    g.drawRect(cellBounds, 1);
}

void TrackerPatternComponent::drawSelection(juce::Graphics& g)
{
    g.setColour(juce::Colour(TrackerColors::selectionHighlight));
    
    int startRow = juce::jmin(selectionStartRow, selectionEndRow);
    int endRow = juce::jmax(selectionStartRow, selectionEndRow);
    int startCh = juce::jmin(selectionStartChannel, selectionEndChannel);
    int endCh = juce::jmax(selectionStartChannel, selectionEndChannel);
    
    for (int row = startRow; row <= endRow; ++row)
    {
        for (int ch = startCh; ch <= endCh; ++ch)
        {
            auto channelBounds = getChannelBounds(ch);
            channelBounds.setY(35 + row * ROW_HEIGHT);
            channelBounds.setHeight(ROW_HEIGHT);
            g.fillRect(channelBounds);
        }
    }
}

void TrackerPatternComponent::drawPlaybackPosition(juce::Graphics& g)
{
    // Draw red playback line like classic trackers
    g.setColour(juce::Colour(TrackerColors::playbackLine));
    
    int playY = 35 + playbackRow * ROW_HEIGHT;
    g.fillRect(0, playY, getWidth(), ROW_HEIGHT);
}

void TrackerPatternComponent::drawGridLines(juce::Graphics& g)
{
    int startY = 35; // After headers
    int startX = 40; // After row numbers
    
    // Draw row grid lines (every 4 rows brighter like beat markers)
    for (int row = 0; row <= TrackerPattern::PATTERN_LENGTH; ++row)
    {
        int rowY = startY + row * ROW_HEIGHT;
        
        if (row % 4 == 0)
        {
            g.setColour(juce::Colour(TrackerColors::beatLines));
            g.drawHorizontalLine(rowY, static_cast<float>(startX), static_cast<float>(getWidth()));
        }
        else
        {
            g.setColour(juce::Colour(TrackerColors::gridLines));
            g.drawHorizontalLine(rowY, static_cast<float>(startX), static_cast<float>(getWidth()));
        }
    }
    
    // Draw channel separator lines
    g.setColour(juce::Colour(TrackerColors::gridLines));
    for (int ch = 0; ch <= channelCount; ++ch)
    {
        int channelX = startX + ch * (CHANNEL_WIDTH * CHAR_WIDTH);
        g.drawVerticalLine(channelX, static_cast<float>(startY), static_cast<float>(getHeight()));
    }
}

void TrackerPatternComponent::drawStatusBar(juce::Graphics& g)
{
    g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain));
    g.setColour(juce::Colour(TrackerColors::textDefault));
    
    int statusY = getHeight() - 25;
    
    // Pattern info
    juce::String patternInfo = "PAT:" + juce::String(currentPatternIndex).paddedLeft('0', 2) + 
                              " ROW:" + juce::String(cursorRow).paddedLeft('0', 2) +
                              " OCT:" + juce::String(currentOctave) +
                              " STP:" + juce::String(editStep);
    
    g.drawText(patternInfo, 10, statusY, 200, 20, juce::Justification::centredLeft, false);
    
    // Channel info
    juce::String channelInfo = "CH:" + juce::String(cursorChannel + 1).paddedLeft('0', 2) + "/" + juce::String(channelCount);
    g.drawText(channelInfo, 250, statusY, 100, 20, juce::Justification::centredLeft, false);
    
    // Current pattern name
    g.drawText(getCurrentPattern().patternName, getWidth() - 150, statusY, 140, 20, 
               juce::Justification::centredRight, false);
}

//==============================================================================
// Revolutionary Input Handling
//==============================================================================

bool TrackerPatternComponent::keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent)
{
    // Handle navigation keys
    if (key == juce::KeyPress::upKey || key == juce::KeyPress::downKey ||
        key == juce::KeyPress::leftKey || key == juce::KeyPress::rightKey ||
        key == juce::KeyPress::pageUpKey || key == juce::KeyPress::pageDownKey ||
        key == juce::KeyPress::homeKey || key == juce::KeyPress::endKey)
    {
        handleNavigationKey(key);
        return true;
    }
    
    // Handle note input (Q-P row for notes like FastTracker2)
    if (cursorColumn == 0) // Note column
    {
        // QWERTY keyboard note mapping like FastTracker2
        static const std::map<int, int> keyToNote = {
            {'q', 0}, {'2', 1}, {'w', 2}, {'3', 3}, {'e', 4}, {'r', 5}, {'5', 6}, {'t', 7},
            {'6', 8}, {'y', 9}, {'7', 10}, {'u', 11}, {'i', 12}, {'9', 13}, {'o', 14}, {'0', 15}, {'p', 16}
        };
        
        auto it = keyToNote.find(tolower(key.getKeyCode()));
        if (it != keyToNote.end())
        {
            int midiNote = it->second + (currentOctave * 12);
            handleNoteInput(midiNote);
            return true;
        }
    }
    
    // Handle hexadecimal input for other columns
    if (cursorColumn > 0)
    {
        char c = static_cast<char>(key.getKeyCode());
        if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))
        {
            int hexValue = charToHex(c);
            handleHexInput(hexValue);
            return true;
        }
    }
    
    // Handle edit commands
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey ||
        key == juce::KeyPress::insertKey || key.getModifiers().isCtrlDown())
    {
        handleEditCommand(key);
        return true;
    }
    
    return false;
}

void TrackerPatternComponent::handleNoteInput(int midiNote)
{
    // Clamp to valid range (C-0 to B-7)
    midiNote = juce::jlimit(0, 95, midiNote);
    
    setNote(cursorRow, cursorChannel, midiNote + 1); // +1 because 0 = empty
    
    // Advance cursor by edit step
    moveCursor(editStep, 0, 0);
    repaint();
}

void TrackerPatternComponent::handleHexInput(int hexValue)
{
    TrackerNote& note = getNoteAt(cursorRow, cursorChannel);
    
    switch (cursorColumn)
    {
    case 1: // Instrument (first digit)
        note.instrument = (note.instrument & 0x0F) | (hexValue << 4);
        if (note.instrument > 99) note.instrument = 99;
        break;
        
    case 2: // Volume
        note.volume = (note.volume & 0x0F) | (hexValue << 4);
        if (note.volume > 64) note.volume = 64;
        break;
        
    case 3: // Effect
        note.effect = hexValue;
        break;
        
    case 4: // Effect parameter
        note.effectParam = (note.effectParam & 0x0F) | (hexValue << 4);
        break;
    }
    
    // Advance cursor to next position
    moveCursor(editStep, 0, 0);
    repaint();
}

void TrackerPatternComponent::handleNavigationKey(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::upKey)
        moveCursor(-1, 0);
    else if (key == juce::KeyPress::downKey)
        moveCursor(1, 0);
    else if (key == juce::KeyPress::leftKey)
        moveCursor(0, 0, -1);
    else if (key == juce::KeyPress::rightKey)
        moveCursor(0, 0, 1);
    else if (key == juce::KeyPress::pageUpKey)
        moveCursor(-16, 0);
    else if (key == juce::KeyPress::pageDownKey)
        moveCursor(16, 0);
    else if (key == juce::KeyPress::homeKey)
        moveCursor(-cursorRow, 0);
    else if (key == juce::KeyPress::endKey)
        moveCursor(TrackerPattern::PATTERN_LENGTH - 1 - cursorRow, 0);
    
    repaint();
}

void TrackerPatternComponent::handleEditCommand(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::deleteKey)
    {
        getNoteAt(cursorRow, cursorChannel).clear();
        repaint();
    }
    else if (key == juce::KeyPress::insertKey)
    {
        insertRow();
    }
    else if (key.getModifiers().isCtrlDown())
    {
        if (key.getKeyCode() == 'c' || key.getKeyCode() == 'C')
            copySelection();
        else if (key.getKeyCode() == 'v' || key.getKeyCode() == 'V')
            pasteSelection();
    }
}

void TrackerPatternComponent::moveCursor(int deltaRow, int deltaChannel, int deltaColumn)
{
    cursorRow = juce::jlimit(0, TrackerPattern::PATTERN_LENGTH - 1, cursorRow + deltaRow);
    cursorChannel = juce::jlimit(0, channelCount - 1, cursorChannel + deltaChannel);
    cursorColumn = juce::jlimit(0, 4, cursorColumn + deltaColumn);
}

//==============================================================================
// Revolutionary Text Conversion
//==============================================================================

juce::String TrackerPatternComponent::noteToString(int note) const
{
    if (note == 0) return "---";
    
    note--; // Convert from 1-based to 0-based
    
    static const char* noteNames[] = {"C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-"};
    
    int octave = note / 12;
    int noteIndex = note % 12;
    
    return juce::String(noteNames[noteIndex]) + juce::String(octave);
}

juce::String TrackerPatternComponent::instrumentToString(int instrument) const
{
    if (instrument == 0) return "--";
    return juce::String(instrument).paddedLeft('0', 2);
}

juce::String TrackerPatternComponent::volumeToString(int volume) const
{
    if (volume == 0) return "--";
    return juce::String(volume).paddedLeft('0', 2);
}

juce::String TrackerPatternComponent::effectToString(int effect) const
{
    if (effect == 0) return "-";
    if (effect <= 9) return juce::String(effect);
    return juce::String(static_cast<char>('A' + effect - 10));
}

juce::String TrackerPatternComponent::paramToString(int param) const
{
    if (param == 0) return "--";
    return juce::String::toHexString(param).toUpperCase().paddedLeft('0', 2);
}

int TrackerPatternComponent::charToHex(juce::juce_wchar c) const
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;
}

//==============================================================================
// Revolutionary Coordinate Conversion
//==============================================================================

juce::Rectangle<int> TrackerPatternComponent::getCellBounds(int row, int channel, int column) const
{
    int startY = 35; // After headers
    int startX = 40; // After row numbers
    
    int rowY = startY + row * ROW_HEIGHT;
    int channelX = startX + channel * (CHANNEL_WIDTH * CHAR_WIDTH);
    
    // Calculate column offset within channel
    int colX = channelX;
    int colWidth = 0;
    
    switch (column)
    {
    case 0: // Note
        colWidth = NOTE_WIDTH * CHAR_WIDTH;
        break;
    case 1: // Instrument
        colX += NOTE_WIDTH * CHAR_WIDTH + COLUMN_SPACING;
        colWidth = INSTRUMENT_WIDTH * CHAR_WIDTH;
        break;
    case 2: // Volume
        colX += (NOTE_WIDTH + INSTRUMENT_WIDTH) * CHAR_WIDTH + COLUMN_SPACING * 2;
        colWidth = VOLUME_WIDTH * CHAR_WIDTH;
        break;
    case 3: // Effect
        colX += (NOTE_WIDTH + INSTRUMENT_WIDTH + VOLUME_WIDTH) * CHAR_WIDTH + COLUMN_SPACING * 3;
        colWidth = EFFECT_WIDTH * CHAR_WIDTH;
        break;
    case 4: // Parameter
        colX += (NOTE_WIDTH + INSTRUMENT_WIDTH + VOLUME_WIDTH + EFFECT_WIDTH) * CHAR_WIDTH + COLUMN_SPACING * 4;
        colWidth = PARAM_WIDTH * CHAR_WIDTH;
        break;
    }
    
    return juce::Rectangle<int>(colX, rowY, colWidth, ROW_HEIGHT);
}

juce::Rectangle<int> TrackerPatternComponent::getChannelBounds(int channel) const
{
    int startX = 40; // After row numbers
    int channelX = startX + channel * (CHANNEL_WIDTH * CHAR_WIDTH);
    return juce::Rectangle<int>(channelX, 0, CHANNEL_WIDTH * CHAR_WIDTH, getHeight());
}

TrackerNote& TrackerPatternComponent::getNoteAt(int row, int channel)
{
    return patterns[currentPatternIndex].notes[channel][row];
}

const TrackerNote& TrackerPatternComponent::getNoteAt(int row, int channel) const
{
    return patterns[currentPatternIndex].notes[channel][row];
}

//==============================================================================
// Revolutionary Pattern Operations
//==============================================================================

void TrackerPatternComponent::clearPattern()
{
    getCurrentPattern().clear();
    repaint();
}

void TrackerPatternComponent::clearChannel(int channel)
{
    if (channel >= 0 && channel < channelCount)
    {
        for (int row = 0; row < TrackerPattern::PATTERN_LENGTH; ++row)
        {
            getNoteAt(row, channel).clear();
        }
        repaint();
    }
}

void TrackerPatternComponent::insertRow()
{
    // TODO: Implement row insertion
    repaint();
}

void TrackerPatternComponent::deleteRow()
{
    // TODO: Implement row deletion
    repaint();
}

void TrackerPatternComponent::copySelection()
{
    // TODO: Implement copy
}

void TrackerPatternComponent::pasteSelection()
{
    // TODO: Implement paste
}

void TrackerPatternComponent::transposeSelection(int semitones)
{
    // TODO: Implement transposition
}

void TrackerPatternComponent::setNote(int row, int channel, int note, int instrument)
{
    if (row >= 0 && row < TrackerPattern::PATTERN_LENGTH && channel >= 0 && channel < channelCount)
    {
        TrackerNote& n = getNoteAt(row, channel);
        n.note = note;
        if (instrument >= 0)
            n.instrument = instrument;
    }
}

//==============================================================================
// Component Overrides
//==============================================================================

void TrackerPatternComponent::resized()
{
    // Calculate optimal size based on channel count
    int minWidth = 40 + channelCount * (CHANNEL_WIDTH * CHAR_WIDTH) + 20;
    int minHeight = 35 + TrackerPattern::PATTERN_LENGTH * ROW_HEIGHT + 30;
    
    setSize(juce::jmax(getWidth(), minWidth), juce::jmax(getHeight(), minHeight));
}

void TrackerPatternComponent::mouseDown(const juce::MouseEvent& e)
{
    int row = getRowFromY(e.y);
    int channel = getChannelFromX(e.x);
    int column = getColumnFromX(e.x, channel);
    
    if (row >= 0 && channel >= 0 && column >= 0)
    {
        cursorRow = row;
        cursorChannel = channel;
        cursorColumn = column;
        
        // Start selection if shift is held
        if (e.mods.isShiftDown())
        {
            if (!hasSelection)
            {
                hasSelection = true;
                selectionStartRow = selectionEndRow = cursorRow;
                selectionStartChannel = selectionEndChannel = cursorChannel;
            }
            updateSelection(cursorRow, cursorChannel);
        }
        else
        {
            hasSelection = false;
        }
        
        repaint();
        grabKeyboardFocus();
    }
}

void TrackerPatternComponent::mouseUp(const juce::MouseEvent& e)
{
    // Nothing special needed on mouse up
}

void TrackerPatternComponent::mouseMove(const juce::MouseEvent& e)
{
    // Could show hover effects here
}

void TrackerPatternComponent::mouseDrag(const juce::MouseEvent& e)
{
    // Handle selection dragging
    if (hasSelection)
    {
        int row = getRowFromY(e.y);
        int channel = getChannelFromX(e.x);
        
        if (row >= 0 && channel >= 0)
        {
            updateSelection(row, channel);
            repaint();
        }
    }
}

void TrackerPatternComponent::updateSelection(int newRow, int newChannel)
{
    selectionEndRow = newRow;
    selectionEndChannel = newChannel;
}

int TrackerPatternComponent::getRowFromY(int y) const
{
    int startY = 35; // After headers
    if (y < startY) return -1;
    
    int row = (y - startY) / ROW_HEIGHT;
    return (row >= 0 && row < TrackerPattern::PATTERN_LENGTH) ? row : -1;
}

int TrackerPatternComponent::getChannelFromX(int x) const
{
    int startX = 40; // After row numbers
    if (x < startX) return -1;
    
    int channel = (x - startX) / (CHANNEL_WIDTH * CHAR_WIDTH);
    return (channel >= 0 && channel < channelCount) ? channel : -1;
}

int TrackerPatternComponent::getColumnFromX(int x, int channel) const
{
    if (channel < 0) return -1;
    
    int startX = 40; // After row numbers
    int channelX = startX + channel * (CHANNEL_WIDTH * CHAR_WIDTH);
    int relativeX = x - channelX;
    
    // Determine which column within the channel
    if (relativeX < NOTE_WIDTH * CHAR_WIDTH) return 0; // Note
    relativeX -= NOTE_WIDTH * CHAR_WIDTH + COLUMN_SPACING;
    
    if (relativeX < INSTRUMENT_WIDTH * CHAR_WIDTH) return 1; // Instrument
    relativeX -= INSTRUMENT_WIDTH * CHAR_WIDTH + COLUMN_SPACING;
    
    if (relativeX < VOLUME_WIDTH * CHAR_WIDTH) return 2; // Volume
    relativeX -= VOLUME_WIDTH * CHAR_WIDTH + COLUMN_SPACING;
    
    if (relativeX < EFFECT_WIDTH * CHAR_WIDTH) return 3; // Effect
    relativeX -= EFFECT_WIDTH * CHAR_WIDTH + COLUMN_SPACING;
    
    if (relativeX < PARAM_WIDTH * CHAR_WIDTH) return 4; // Parameter
    
    return -1;
}

//==============================================================================
// Timer Callback - Cursor Blinking
//==============================================================================

void TrackerPatternComponent::timerCallback()
{
    cursorVisible = !cursorVisible;
    repaint();
}

//==============================================================================
// Public Interface Functions
//==============================================================================

void TrackerPatternComponent::setChannelCount(int channels)
{
    channelCount = juce::jlimit(1, TrackerPattern::MAX_CHANNELS, channels);
    cursorChannel = juce::jmin(cursorChannel, channelCount - 1);
    resized();
    repaint();
}

void TrackerPatternComponent::setCurrentPattern(int patternIndex)
{
    currentPatternIndex = juce::jlimit(0, MAX_PATTERNS - 1, patternIndex);
    repaint();
}

void TrackerPatternComponent::setPlaybackPosition(int row)
{
    playbackRow = row;
    isPlaying = (row >= 0);
    repaint();
}

bool TrackerPatternComponent::keyStateChanged(bool isKeyDown, juce::Component* originatingComponent)
{
    return false; // Let other handlers process this
}