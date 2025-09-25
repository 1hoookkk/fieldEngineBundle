/******************************************************************************
 * File: TrackerPatternComponent.h
 * Description: Revolutionary FastTracker2/ProTracker-style pattern editor
 * 
 * REVOLUTIONARY TRACKER INTERFACE - FastTracker2/ProTracker Style
 * Authentic hexadecimal pattern editing with chunky pixelated display
 * 
 * Copyright (c) 2025 Spectral Audio Systems
 ******************************************************************************/

#pragma once
#include <JuceHeader.h>
#include "RetroBrutalistLookAndFeel.h"

/**
 * @brief TrackerNote - Individual pattern cell in authentic tracker format
 */
struct TrackerNote
{
    int note = 0;           // 0=empty, 1-96=C-0 to B-7
    int instrument = 0;     // 0=empty, 1-99=instrument number
    int volume = 0;         // 0=empty, 1-64=volume
    int effect = 0;         // 0=empty, 1-35=effect command (A-Z, 0-9)
    int effectParam = 0;    // 0-255=effect parameter
    
    bool isEmpty() const { return note == 0 && instrument == 0 && volume == 0 && effect == 0; }
    void clear() { note = instrument = volume = effect = effectParam = 0; }
};

/**
 * @brief TrackerPattern - 64-row pattern with multiple channels
 */
struct TrackerPattern
{
    static constexpr int PATTERN_LENGTH = 64;
    static constexpr int MAX_CHANNELS = 32;
    
    TrackerNote notes[MAX_CHANNELS][PATTERN_LENGTH];
    juce::String patternName = "UNTITLED";
    
    void clear()
    {
        for (int ch = 0; ch < MAX_CHANNELS; ++ch)
            for (int row = 0; row < PATTERN_LENGTH; ++row)
                notes[ch][row].clear();
    }
};

/**
 * @brief Revolutionary Tracker Pattern Editor Component
 * 
 * Authentic FastTracker2/ProTracker interface with:
 * - Hexadecimal pattern editing
 * - VGA-16 color scheme
 * - Chunky pixelated fonts
 * - Professional tracker key commands
 */
class TrackerPatternComponent : public juce::Component,
                               public juce::KeyListener,
                               public juce::Timer
{
public:
    TrackerPatternComponent();
    ~TrackerPatternComponent() override;
    
    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    
    // KeyListener overrides for tracker key commands
    bool keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent) override;
    bool keyStateChanged(bool isKeyDown, juce::Component* originatingComponent) override;
    
    // Timer for cursor blinking
    void timerCallback() override;
    
    // Revolutionary tracker interface
    void setChannelCount(int channels);
    void setCurrentPattern(int patternIndex);
    void setPlaybackPosition(int row);
    void setOctave(int octave) { currentOctave = juce::jlimit(0, 7, octave); }
    void setStep(int step) { editStep = juce::jlimit(0, 16, step); }
    
    // Pattern data access
    TrackerPattern& getCurrentPattern() { return patterns[currentPatternIndex]; }
    const TrackerPattern& getCurrentPattern() const { return patterns[currentPatternIndex]; }
    
    // Professional tracker functions
    void clearPattern();
    void clearChannel(int channel);
    void insertRow();
    void deleteRow();
    void copySelection();
    void pasteSelection();
    void transposeSelection(int semitones);

private:
    //==============================================================================
    // Revolutionary Tracker Data
    
    static constexpr int MAX_PATTERNS = 128;
    TrackerPattern patterns[MAX_PATTERNS];
    int currentPatternIndex = 0;
    int channelCount = 8; // Start with 8 channels like old trackers
    
    //==============================================================================
    // Cursor and Edit State
    
    int cursorRow = 0;
    int cursorChannel = 0;
    int cursorColumn = 0; // 0=note, 1=instrument, 2=volume, 3=effect, 4=param
    int currentOctave = 4;
    int editStep = 1; // How many rows to advance after note entry
    bool cursorVisible = true;
    
    //==============================================================================
    // Selection State
    
    bool hasSelection = false;
    int selectionStartRow = 0;
    int selectionEndRow = 0;
    int selectionStartChannel = 0;
    int selectionEndChannel = 0;
    
    //==============================================================================
    // Playback State
    
    int playbackRow = -1; // -1 = not playing
    bool isPlaying = false;
    
    //==============================================================================
    // Revolutionary Visual Configuration
    
    struct TrackerColors
    {
        static constexpr juce::uint32 backgroundBlack = 0xFF000000;     // Pure black
        static constexpr juce::uint32 textDefault = 0xFFAAAAAA;        // Light gray text
        static constexpr juce::uint32 textNote = 0xFFFFFFFF;           // White notes
        static constexpr juce::uint32 textInstrument = 0xFFFFFF00;     // Yellow instruments
        static constexpr juce::uint32 textVolume = 0xFF00FFFF;         // Cyan volume
        static constexpr juce::uint32 textEffect = 0xFF00FF00;         // Green effects
        static constexpr juce::uint32 rowNumbers = 0xFF888888;         // Gray row numbers
        static constexpr juce::uint32 channelHeaders = 0xFFFFFFFF;     // White channel headers
        static constexpr juce::uint32 cursorHighlight = 0xFF0000FF;    // Blue cursor
        static constexpr juce::uint32 selectionHighlight = 0xFF444444; // Dark gray selection
        static constexpr juce::uint32 playbackLine = 0xFFFF0000;       // Red playback line
        static constexpr juce::uint32 gridLines = 0xFF333333;          // Dark grid
        static constexpr juce::uint32 beatLines = 0xFF555555;          // Brighter beat lines
    };
    
    //==============================================================================
    // Revolutionary Layout Metrics
    
    static constexpr int CHAR_WIDTH = 8;   // Pixelated character width
    static constexpr int CHAR_HEIGHT = 14; // Pixelated character height
    static constexpr int ROW_HEIGHT = 16;  // Row spacing
    static constexpr int COLUMN_SPACING = 4; // Space between columns
    
    // Column widths in characters
    static constexpr int NOTE_WIDTH = 3;        // "C-4"
    static constexpr int INSTRUMENT_WIDTH = 2;  // "01"
    static constexpr int VOLUME_WIDTH = 2;      // "40"
    static constexpr int EFFECT_WIDTH = 1;      // "A"
    static constexpr int PARAM_WIDTH = 2;       // "00"
    
    static constexpr int CHANNEL_WIDTH = NOTE_WIDTH + INSTRUMENT_WIDTH + VOLUME_WIDTH + EFFECT_WIDTH + PARAM_WIDTH + 4; // 13 chars + spaces
    
    //==============================================================================
    // Revolutionary Rendering Functions
    
    void drawTrackerBackground(juce::Graphics& g);
    void drawChannelHeaders(juce::Graphics& g);
    void drawRowNumbers(juce::Graphics& g);
    void drawPatternData(juce::Graphics& g);
    void drawCursor(juce::Graphics& g);
    void drawSelection(juce::Graphics& g);
    void drawPlaybackPosition(juce::Graphics& g);
    void drawGridLines(juce::Graphics& g);
    void drawStatusBar(juce::Graphics& g);
    
    //==============================================================================
    // Revolutionary Input Handling
    
    void handleNoteInput(int midiNote);
    void handleHexInput(int hexValue);
    void handleNavigationKey(const juce::KeyPress& key);
    void handleEditCommand(const juce::KeyPress& key);
    void moveCursor(int deltaRow, int deltaChannel, int deltaColumn = 0);
    void updateSelection(int newRow, int newChannel);
    
    //==============================================================================
    // Revolutionary Text Conversion
    
    juce::String noteToString(int note) const;
    juce::String instrumentToString(int instrument) const;
    juce::String volumeToString(int volume) const;
    juce::String effectToString(int effect) const;
    juce::String paramToString(int param) const;
    
    int stringToNote(const juce::String& str) const;
    int charToHex(juce::juce_wchar c) const;
    
    //==============================================================================
    // Revolutionary Coordinate Conversion
    
    int getRowFromY(int y) const;
    int getChannelFromX(int x) const;
    int getColumnFromX(int x, int channel) const;
    juce::Rectangle<int> getCellBounds(int row, int channel, int column) const;
    juce::Rectangle<int> getChannelBounds(int channel) const;
    
    //==============================================================================
    // Revolutionary Pattern Operations
    
    void setNote(int row, int channel, int note, int instrument = -1);
    void setInstrument(int row, int channel, int instrument);
    void setVolume(int row, int channel, int volume);
    void setEffect(int row, int channel, int effect, int param = -1);
    
    TrackerNote& getNoteAt(int row, int channel);
    const TrackerNote& getNoteAt(int row, int channel) const;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackerPatternComponent)
};