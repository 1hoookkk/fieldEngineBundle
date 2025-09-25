/******************************************************************************
 * File: TrackerDrumSequencer.h
 * Description: Revolutionary tracker-style linear drumming sequencer
 * 
 * SPECTRAL CANVAS PRO - Innovative drum programming with classic tracker workflow
 * Implements the revolutionary "TRACKER STYLE LINEAR DRUMMING TECHNIQUE" 
 * requested by the user for efficient beat creation.
 * 
 * Copyright (c) 2025 Spectral Audio Systems
 ******************************************************************************/

#pragma once
#include <JuceHeader.h>
#include "VintageProLookAndFeel.h"

/**
 * @brief Pattern data structure for tracker-style drum programming
 * 
 * Represents a single pattern in the tracker with multiple channels
 * for different drum sounds and effect columns.
 */
class TrackerPattern
{
public:
    static constexpr int MAX_CHANNELS = 16;      // Max drum channels
    static constexpr int MAX_LINES = 64;         // Lines per pattern
    static constexpr int TICKS_PER_LINE = 6;     // Resolution
    
    struct Note
    {
        uint8_t note = 0xFF;        // 0xFF = empty, 0-127 = MIDI note
        uint8_t instrument = 0xFF;   // Drum instrument index
        uint8_t volume = 0xFF;       // 0x00-0x40 volume, 0xFF = no change
        uint8_t effect = 0x00;       // Effect command
        uint8_t effectParam = 0x00;  // Effect parameter
        
        bool isEmpty() const { return note == 0xFF; }
        void clear() { note = 0xFF; instrument = 0xFF; volume = 0xFF; effect = 0; effectParam = 0; }
    };
    
    TrackerPattern(int numLines = 16);
    
    Note& getNote(int channel, int line);
    const Note& getNote(int channel, int line) const;
    
    void setLength(int numLines);
    int getLength() const { return lines; }
    
    void clear();
    void clearChannel(int channel);
    
private:
    std::vector<std::vector<Note>> data;
    int lines;
};

/**
 * @brief Tracker-style drum sequencer component
 * 
 * Revolutionary interface for linear drum programming inspired by
 * classic trackers like FastTracker II and Impulse Tracker.
 */
class TrackerDrumSequencer : public juce::Component,
                            public juce::Timer,
                            private juce::ScrollBar::Listener
{
public:
    TrackerDrumSequencer();
    ~TrackerDrumSequencer() override;
    
    //==============================================================================
    // Pattern Management
    
    void setCurrentPattern(int patternIndex);
    int getCurrentPattern() const { return currentPatternIndex; }
    
    TrackerPattern* getPattern(int index);
    void addNewPattern();
    void duplicatePattern(int sourceIndex);
    void clearPattern(int index);
    
    //==============================================================================
    // Playback Control
    
    void play();
    void stop();
    void pause();
    bool isPlaying() const { return playing; }
    
    void setTempo(double bpm) { tempoBPM = bpm; }
    double getTempo() const { return tempoBPM; }
    
    void setPlaybackPosition(int line);
    int getPlaybackPosition() const { return currentLine; }
    
    //==============================================================================
    // Drum Instruments
    
    struct DrumInstrument
    {
        juce::String name;
        juce::Colour color;
        int midiNote;
        float volume;
        float pan;
    };
    
    void addDrumInstrument(const DrumInstrument& instrument);
    void removeDrumInstrument(int index);
    DrumInstrument& getDrumInstrument(int index);
    int getNumDrumInstruments() const { return drumInstruments.size(); }
    
    //==============================================================================
    // Component Interface
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    bool keyPressed(const juce::KeyPress& key) override;
    
    //==============================================================================
    // Professional Features
    
    void setEditStep(int step) { editStep = juce::jlimit(0, 16, step); }
    int getEditStep() const { return editStep; }
    
    void setOctave(int oct) { currentOctave = juce::jlimit(0, 8, oct); }
    int getOctave() const { return currentOctave; }
    
    void enableRecording(bool enable) { recording = enable; }
    bool isRecording() const { return recording; }
    
private:
    //==============================================================================
    // Pattern Data
    
    std::vector<std::unique_ptr<TrackerPattern>> patterns;
    int currentPatternIndex = 0;
    
    //==============================================================================
    // Playback State
    
    bool playing = false;
    bool recording = false;
    int currentLine = 0;
    int currentTick = 0;
    double tempoBPM = 120.0;
    
    //==============================================================================
    // Editing State
    
    int cursorChannel = 0;
    int cursorLine = 0;
    int cursorColumn = 0;  // 0=note, 1=instrument, 2=volume, 3=effect, 4=param
    int editStep = 1;
    int currentOctave = 4;
    int selectedInstrument = 0;
    
    //==============================================================================
    // Visual Configuration
    
    static constexpr int LINE_HEIGHT = 14;
    static constexpr int CHANNEL_WIDTH = 120;
    static constexpr int LINE_NUMBER_WIDTH = 40;
    static constexpr int HEADER_HEIGHT = 30;
    
    juce::Font trackerFont;
    juce::ScrollBar verticalScrollBar;
    int visibleStartLine = 0;
    int visibleLines = 0;
    
    //==============================================================================
    // Drum Instruments
    
    std::vector<DrumInstrument> drumInstruments;
    void initializeDefaultDrumKit();
    
    //==============================================================================
    // Drawing Methods
    
    void drawBackground(juce::Graphics& g);
    void drawHeader(juce::Graphics& g);
    void drawLineNumbers(juce::Graphics& g);
    void drawPatternData(juce::Graphics& g);
    void drawCursor(juce::Graphics& g);
    void drawPlaybackPosition(juce::Graphics& g);
    
    juce::Rectangle<int> getChannelBounds(int channel) const;
    juce::Rectangle<int> getLineBounds(int line) const;
    juce::Rectangle<int> getCellBounds(int channel, int line, int column) const;
    
    juce::String getNoteText(uint8_t note) const;
    juce::String getHexText(uint8_t value) const;
    
    //==============================================================================
    // Interaction
    
    void moveCursor(int deltaChannel, int deltaLine, int deltaColumn);
    void enterNoteAtCursor(int note);
    void deleteNoteAtCursor();
    void handleTrackerKeyPress(const juce::KeyPress& key);
    
    //==============================================================================
    // Playback Engine
    
    void timerCallback() override;
    void advancePlayback();
    void triggerNotesOnLine(int line);
    
    //==============================================================================
    // Scrolling
    
    void scrollBarMoved(juce::ScrollBar* scrollBar, double newRangeStart) override;
    void updateScrollBar();
    void ensureCursorVisible();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackerDrumSequencer)
};