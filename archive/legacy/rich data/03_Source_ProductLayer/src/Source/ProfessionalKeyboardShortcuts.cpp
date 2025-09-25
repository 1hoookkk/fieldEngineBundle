/******************************************************************************
 * File: ProfessionalKeyboardShortcuts.cpp
 * Description: Implementation of professional keyboard shortcuts system
 * 
 * Industry-standard DAW keyboard shortcuts with full customization support.
 * Provides efficient workflow for professional audio production.
 * 
 * Copyright (c) 2025 Spectral Audio Systems
 ******************************************************************************/

#include "ProfessionalKeyboardShortcuts.h"

//==============================================================================
// Constructor
//==============================================================================

ProfessionalKeyboardShortcuts::ProfessionalKeyboardShortcuts()
{
    registerAllCommands();
    setupDefaultKeyMappings();
}

//==============================================================================
// Command Registration
//==============================================================================

void ProfessionalKeyboardShortcuts::registerAllCommands()
{
    // Register all professional DAW commands
    setupTransportShortcuts();
    setupFileShortcuts();
    setupEditShortcuts();
    setupCanvasShortcuts();
    setupTrackerShortcuts();
    setupViewShortcuts();
}

void ProfessionalKeyboardShortcuts::setupDefaultKeyMappings()
{
    // Clear existing mappings
    getKeyMappings()->resetToDefaultMappings();
    
    auto shortcuts = getProfessionalShortcuts();
    
    for (const auto& shortcut : shortcuts)
    {
        getKeyMappings()->addKeyPress(shortcut.commandID, shortcut.defaultKey);
    }
}

//==============================================================================
// Professional Shortcut Definitions
//==============================================================================

std::vector<ProfessionalKeyboardShortcuts::ShortcutDefinition> 
ProfessionalKeyboardShortcuts::getProfessionalShortcuts()
{
    using namespace CommandIDs;
    
    return {
        // Transport Controls (Industry Standard)
        { playPause, "Play/Pause", "Transport", juce::KeyPress(juce::KeyPress::spaceKey), true },
        { stop, "Stop", "Transport", juce::KeyPress('s', juce::ModifierKeys::noModifiers, 0), true },
        { record, "Record", "Transport", juce::KeyPress('r', juce::ModifierKeys::noModifiers, 0), true },
        { rewind, "Rewind", "Transport", juce::KeyPress(juce::KeyPress::leftKey), true },
        { fastForward, "Fast Forward", "Transport", juce::KeyPress(juce::KeyPress::rightKey), true },
        { returnToZero, "Return to Zero", "Transport", juce::KeyPress(juce::KeyPress::homeKey), true },
        
        // File Operations (Standard DAW)
        { newProject, "New Project", "File", juce::KeyPress('n', juce::ModifierKeys::ctrlModifier, 0), true },
        { openProject, "Open Project", "File", juce::KeyPress('o', juce::ModifierKeys::ctrlModifier, 0), true },
        { saveProject, "Save Project", "File", juce::KeyPress('s', juce::ModifierKeys::ctrlModifier, 0), true },
        { saveProjectAs, "Save Project As", "File", juce::KeyPress('s', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 0), true },
        { exportAudio, "Export Audio", "File", juce::KeyPress('e', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 0), true },
        { importSample, "Import Sample", "File", juce::KeyPress('i', juce::ModifierKeys::ctrlModifier, 0), true },
        
        // Edit Operations (Universal)
        { undo, "Undo", "Edit", juce::KeyPress('z', juce::ModifierKeys::ctrlModifier, 0), true },
        { redo, "Redo", "Edit", juce::KeyPress('y', juce::ModifierKeys::ctrlModifier, 0), true },
        { cut, "Cut", "Edit", juce::KeyPress('x', juce::ModifierKeys::ctrlModifier, 0), true },
        { copy, "Copy", "Edit", juce::KeyPress('c', juce::ModifierKeys::ctrlModifier, 0), true },
        { paste, "Paste", "Edit", juce::KeyPress('v', juce::ModifierKeys::ctrlModifier, 0), true },
        { deleteSelection, "Delete", "Edit", juce::KeyPress(juce::KeyPress::deleteKey), true },
        { selectAll, "Select All", "Edit", juce::KeyPress('a', juce::ModifierKeys::ctrlModifier, 0), true },
        { deselectAll, "Deselect All", "Edit", juce::KeyPress('d', juce::ModifierKeys::ctrlModifier, 0), true },
        
        // Canvas Operations (Paint Mode)
        { paintMode, "Paint Mode", "Canvas", juce::KeyPress('p', juce::ModifierKeys::noModifiers, 0), false },
        { eraseMode, "Erase Mode", "Canvas", juce::KeyPress('e', juce::ModifierKeys::noModifiers, 0), false },
        { selectMode, "Select Mode", "Canvas", juce::KeyPress('v', juce::ModifierKeys::noModifiers, 0), false },
        { zoomIn, "Zoom In", "Canvas", juce::KeyPress('+', juce::ModifierKeys::ctrlModifier, 0), false },
        { zoomOut, "Zoom Out", "Canvas", juce::KeyPress('-', juce::ModifierKeys::ctrlModifier, 0), false },
        { zoomToFit, "Zoom to Fit", "Canvas", juce::KeyPress('0', juce::ModifierKeys::ctrlModifier, 0), false },
        { toggleGrid, "Toggle Grid", "Canvas", juce::KeyPress('g', juce::ModifierKeys::noModifiers, 0), false },
        { toggleSnap, "Toggle Snap", "Canvas", juce::KeyPress('n', juce::ModifierKeys::noModifiers, 0), false },
        
        // Sample Masking (Professional)
        { toggleMaskMode, "Toggle Mask Mode", "Masking", juce::KeyPress('m', juce::ModifierKeys::noModifiers, 0), false },
        { invertMask, "Invert Mask", "Masking", juce::KeyPress('i', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 0), false },
        { clearMask, "Clear Mask", "Masking", juce::KeyPress('m', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 0), false },
        
        // View Controls
        { toggleSpectralView, "Toggle Spectral View", "View", juce::KeyPress(juce::KeyPress::F1), true },
        { toggleWaveformView, "Toggle Waveform View", "View", juce::KeyPress(juce::KeyPress::F2), true },
        { toggleTrackerView, "Toggle Tracker View", "View", juce::KeyPress(juce::KeyPress::F3), true },
        { toggleMixerView, "Toggle Mixer View", "View", juce::KeyPress(juce::KeyPress::F4), true },
        { toggleFullscreen, "Toggle Fullscreen", "View", juce::KeyPress(juce::KeyPress::F11), true },
        
        // Tracker Operations (Classic Style)
        { trackerNoteC, "Note C", "Tracker", juce::KeyPress('z', juce::ModifierKeys::noModifiers, 0), false },
        { trackerNoteCs, "Note C#", "Tracker", juce::KeyPress('s', juce::ModifierKeys::noModifiers, 0), false },
        { trackerNoteD, "Note D", "Tracker", juce::KeyPress('x', juce::ModifierKeys::noModifiers, 0), false },
        { trackerNoteDs, "Note D#", "Tracker", juce::KeyPress('d', juce::ModifierKeys::noModifiers, 0), false },
        { trackerNoteE, "Note E", "Tracker", juce::KeyPress('c', juce::ModifierKeys::noModifiers, 0), false },
        { trackerNoteF, "Note F", "Tracker", juce::KeyPress('v', juce::ModifierKeys::noModifiers, 0), false },
        { trackerNoteFs, "Note F#", "Tracker", juce::KeyPress('g', juce::ModifierKeys::noModifiers, 0), false },
        { trackerNoteG, "Note G", "Tracker", juce::KeyPress('b', juce::ModifierKeys::noModifiers, 0), false },
        { trackerNoteGs, "Note G#", "Tracker", juce::KeyPress('h', juce::ModifierKeys::noModifiers, 0), false },
        { trackerNoteA, "Note A", "Tracker", juce::KeyPress('n', juce::ModifierKeys::noModifiers, 0), false },
        { trackerNoteAs, "Note A#", "Tracker", juce::KeyPress('j', juce::ModifierKeys::noModifiers, 0), false },
        { trackerNoteB, "Note B", "Tracker", juce::KeyPress('m', juce::ModifierKeys::noModifiers, 0), false },
        { trackerOctaveUp, "Octave Up", "Tracker", juce::KeyPress(juce::KeyPress::upKey), false },
        { trackerOctaveDown, "Octave Down", "Tracker", juce::KeyPress(juce::KeyPress::downKey), false },
        { trackerNextLine, "Next Line", "Tracker", juce::KeyPress(juce::KeyPress::returnKey), false },
        
        // Help
        { showHelp, "Show Help", "Help", juce::KeyPress(juce::KeyPress::F1, juce::ModifierKeys::ctrlModifier, 0), true },
        { showShortcuts, "Show Shortcuts", "Help", juce::KeyPress('?', juce::ModifierKeys::shiftModifier, 0), true }
    };
}

//==============================================================================
// Context-Specific Setup
//==============================================================================

void ProfessionalKeyboardShortcuts::setupTransportShortcuts()
{
    // Transport commands are registered globally
    // Implementation handled by MainComponent
}

void ProfessionalKeyboardShortcuts::setupFileShortcuts()
{
    // File operations are registered globally
    // Implementation handled by MainComponent
}

void ProfessionalKeyboardShortcuts::setupEditShortcuts()
{
    // Edit operations are context-sensitive
    // Implementation depends on focused component
}

void ProfessionalKeyboardShortcuts::setupCanvasShortcuts()
{
    // Canvas-specific shortcuts
    // Active only when canvas has focus
}

void ProfessionalKeyboardShortcuts::setupTrackerShortcuts()
{
    // Tracker-specific shortcuts
    // Active only in tracker view
}

void ProfessionalKeyboardShortcuts::setupViewShortcuts()
{
    // View switching shortcuts are global
    // Implementation handled by MainComponent
}

//==============================================================================
// Customization
//==============================================================================

bool ProfessionalKeyboardShortcuts::loadCustomKeyMappings(const juce::File& file)
{
    if (!file.existsAsFile())
        return false;
    
    juce::XmlDocument doc(file);
    auto xml = doc.getDocumentElement();
    
    if (xml == nullptr || xml->getTagName() != "SPECTRALCANVASPRO_SHORTCUTS")
        return false;
    
    getKeyMappings()->restoreFromXml(*xml);
    return true;
}

bool ProfessionalKeyboardShortcuts::saveCustomKeyMappings(const juce::File& file)
{
    auto xml = getKeyMappings()->createXml(true);
    
    if (xml == nullptr)
        return false;
    
    xml->setTagName("SPECTRALCANVASPRO_SHORTCUTS");
    xml->setAttribute("version", "1.0");
    
    return xml->writeTo(file);
}

void ProfessionalKeyboardShortcuts::resetToDefaults()
{
    setupDefaultKeyMappings();
}

//==============================================================================
// Help System
//==============================================================================

juce::String ProfessionalKeyboardShortcuts::generateShortcutReference() const
{
    juce::String reference;
    reference << "SPECTRAL CANVAS PRO - Professional Keyboard Shortcuts\n";
    reference << "====================================================\n\n";
    
    auto shortcuts = getProfessionalShortcuts();
    juce::String currentCategory;
    
    for (const auto& shortcut : shortcuts)
    {
        if (shortcut.category != currentCategory)
        {
            currentCategory = shortcut.category;
            reference << "\n" << currentCategory << ":\n";
            reference << juce::String::repeatedString("-", currentCategory.length() + 1) << "\n";
        }
        
        juce::String keyText = shortcut.defaultKey.getTextDescription();
        reference << juce::String(shortcut.description).paddedRight(' ', 30);
        reference << keyText << "\n";
    }
    
    return reference;
}

std::vector<ProfessionalKeyboardShortcuts::ShortcutDefinition> 
ProfessionalKeyboardShortcuts::getShortcutsForContext(ShortcutContext context) const
{
    auto allShortcuts = getProfessionalShortcuts();
    std::vector<ShortcutDefinition> contextShortcuts;
    
    for (const auto& shortcut : allShortcuts)
    {
        bool includeShortcut = shortcut.isGlobal;
        
        if (!includeShortcut)
        {
            switch (context)
            {
                case ShortcutContext::Canvas:
                    includeShortcut = (shortcut.category == "Canvas" || shortcut.category == "Masking");
                    break;
                case ShortcutContext::Tracker:
                    includeShortcut = (shortcut.category == "Tracker");
                    break;
                default:
                    break;
            }
        }
        
        if (includeShortcut)
            contextShortcuts.push_back(shortcut);
    }
    
    return contextShortcuts;
}

//==============================================================================
// Quick Access Palette Implementation
//==============================================================================

class QuickAccessPalette::PaletteContent : public juce::Component
{
public:
    PaletteContent(juce::ApplicationCommandManager& cmdManager)
        : commandManager(cmdManager)
    {
        setSize(200, 300);
    }
    
    void paint(juce::Graphics& g) override
    {
        // Vintage palette background
        g.fillAll(juce::Colour(0xFF2B2B2B));
        g.setColour(juce::Colour(0xFF404040));
        g.drawRect(getLocalBounds(), 2);
    }
    
    void addCommandButton(int commandID, const juce::String& label)
    {
        auto button = std::make_unique<juce::TextButton>(label);
        button->onClick = [this, commandID]()
        {
            commandManager.invokeDirectly(commandID, false);
        };
        
        addAndMakeVisible(button.get());
        commandButtons.push_back(std::move(button));
        updateLayout();
    }
    
private:
    juce::ApplicationCommandManager& commandManager;
    std::vector<std::unique_ptr<juce::TextButton>> commandButtons;
    
    void updateLayout()
    {
        int y = 10;
        for (auto& button : commandButtons)
        {
            button->setBounds(10, y, getWidth() - 20, 25);
            y += 30;
        }
    }
};

QuickAccessPalette::QuickAccessPalette(juce::ApplicationCommandManager& cmdManager)
    : DocumentWindow("Quick Access", juce::Colours::darkgrey, DocumentWindow::closeButton),
      commandManager(cmdManager)
{
    content = std::make_unique<PaletteContent>(commandManager);
    setContentOwned(content.get(), false);
    setResizable(true, true);
    centreWithSize(200, 300);
}

void QuickAccessPalette::addCommand(int commandID, const juce::String& label)
{
    if (content)
        content->addCommandButton(commandID, label);
}