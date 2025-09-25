/******************************************************************************
 * File: ProfessionalKeyboardShortcuts.h
 * Description: Professional keyboard shortcuts system for SPECTRAL CANVAS PRO
 * 
 * Implements industry-standard DAW keyboard shortcuts with customizable bindings
 * for efficient professional workflow. Inspired by Cool Edit Pro, Pro Tools, 
 * and classic tracker interfaces.
 * 
 * Copyright (c) 2025 Spectral Audio Systems
 ******************************************************************************/

#pragma once
#include <JuceHeader.h>

/**
 * @brief Professional keyboard command IDs for SPECTRAL CANVAS PRO
 * 
 * Comprehensive list of all keyboard-accessible commands following
 * industry-standard DAW conventions for maximum efficiency.
 */
namespace CommandIDs
{
    enum
    {
        // Transport Controls
        playPause = 1000,
        stop,
        record,
        rewind,
        fastForward,
        returnToZero,
        
        // File Operations
        newProject = 2000,
        openProject,
        saveProject,
        saveProjectAs,
        exportAudio,
        importSample,
        
        // Edit Operations
        undo = 3000,
        redo,
        cut,
        copy,
        paste,
        deleteSelection,
        selectAll,
        deselectAll,
        
        // Canvas Operations
        paintMode = 4000,
        eraseMode,
        selectMode,
        zoomIn,
        zoomOut,
        zoomToFit,
        toggleGrid,
        toggleSnap,
        
        // Sample Masking
        toggleMaskMode = 5000,
        invertMask,
        clearMask,
        featherMask,
        growMask,
        shrinkMask,
        
        // View Controls
        toggleSpectralView = 6000,
        toggleWaveformView,
        toggleTrackerView,
        toggleMixerView,
        toggleFullscreen,
        
        // Professional Features
        toggleBypass = 7000,
        toggleSolo,
        toggleMute,
        toggleAutomation,
        insertMarker,
        nextMarker,
        previousMarker,
        
        // Tracker Operations
        trackerNoteC = 8000,
        trackerNoteCs,
        trackerNoteD,
        trackerNoteDs,
        trackerNoteE,
        trackerNoteF,
        trackerNoteFs,
        trackerNoteG,
        trackerNoteGs,
        trackerNoteA,
        trackerNoteAs,
        trackerNoteB,
        trackerOctaveUp,
        trackerOctaveDown,
        trackerNextLine,
        trackerPrevLine,
        trackerNextChannel,
        trackerPrevChannel,
        
        // Help
        showHelp = 9000,
        showShortcuts,
        showAbout
    };
}

/**
 * @brief Professional keyboard shortcuts manager
 * 
 * Handles all keyboard input for SPECTRAL CANVAS PRO with customizable
 * key bindings and context-sensitive command execution.
 */
class ProfessionalKeyboardShortcuts : public juce::ApplicationCommandManager
{
public:
    ProfessionalKeyboardShortcuts();
    ~ProfessionalKeyboardShortcuts() override = default;
    
    //==============================================================================
    // Command Registration and Management
    
    void registerAllCommands();
    void setupDefaultKeyMappings();
    
    //==============================================================================
    // Professional Shortcut Definitions
    
    struct ShortcutDefinition
    {
        int commandID;
        juce::String description;
        juce::String category;
        juce::KeyPress defaultKey;
        bool isGlobal;
    };
    
    static std::vector<ShortcutDefinition> getProfessionalShortcuts();
    
    //==============================================================================
    // Context Management
    
    enum class ShortcutContext
    {
        Global,
        Canvas,
        Tracker,
        Mixer,
        Editor
    };
    
    void setCurrentContext(ShortcutContext context) { currentContext = context; }
    ShortcutContext getCurrentContext() const { return currentContext; }
    
    //==============================================================================
    // Customization
    
    bool loadCustomKeyMappings(const juce::File& file);
    bool saveCustomKeyMappings(const juce::File& file);
    void resetToDefaults();
    
    //==============================================================================
    // Help System
    
    juce::String generateShortcutReference() const;
    std::vector<ShortcutDefinition> getShortcutsForContext(ShortcutContext context) const;
    
private:
    ShortcutContext currentContext = ShortcutContext::Global;
    
    void setupTransportShortcuts();
    void setupFileShortcuts();
    void setupEditShortcuts();
    void setupCanvasShortcuts();
    void setupTrackerShortcuts();
    void setupViewShortcuts();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProfessionalKeyboardShortcuts)
};

/**
 * @brief Quick access palette for frequently used commands
 * 
 * Floating window with customizable buttons for rapid access to
 * professional features without keyboard shortcuts.
 */
class QuickAccessPalette : public juce::DocumentWindow
{
public:
    QuickAccessPalette(juce::ApplicationCommandManager& commandManager);
    ~QuickAccessPalette() override = default;
    
    void addCommand(int commandID, const juce::String& label);
    void removeCommand(int commandID);
    void clearAllCommands();
    
    void saveConfiguration(const juce::File& file);
    void loadConfiguration(const juce::File& file);
    
private:
    class PaletteContent;
    std::unique_ptr<PaletteContent> content;
    juce::ApplicationCommandManager& commandManager;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QuickAccessPalette)
};