/******************************************************************************
 * File: ProfessionalErrorDialog.h
 * Description: Professional error dialog for SPECTRAL CANVAS PRO
 * 
 * Vintage DAW-style error reporting with comprehensive information display.
 * Inspired by classic professional audio software error handling.
 * 
 * Copyright (c) 2025 Spectral Audio Systems
 ******************************************************************************/

#pragma once
#include <JuceHeader.h>
#include "VintageProLookAndFeel.h"

/**
 * @brief Professional error dialog with vintage DAW aesthetics
 * 
 * Displays errors with detailed information in a style consistent with
 * classic professional audio workstations like Cool Edit Pro and Sound Forge.
 */
class ProfessionalErrorDialog : public juce::Component
{
public:
    enum class ErrorType
    {
        FileError,
        AudioError,
        SystemError,
        WarningMessage,
        InfoMessage
    };
    
    ProfessionalErrorDialog(const juce::String& title, 
                           const juce::String& message,
                           ErrorType type = ErrorType::SystemError);
    
    ~ProfessionalErrorDialog() override = default;
    
    //==============================================================================
    // Display Methods
    
    static void showError(const juce::String& title, const juce::String& message);
    static void showFileError(const juce::String& fileName, const juce::String& details);
    static void showWarning(const juce::String& title, const juce::String& message);
    static void showInfo(const juce::String& title, const juce::String& message);
    
    //==============================================================================
    // Component Interface
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    void show();
    void dismiss();
    
private:
    //==============================================================================
    // UI Components
    
    juce::Label titleLabel;
    juce::TextEditor messageEditor;
    juce::TextButton okButton;
    juce::TextButton detailsButton;
    
    //==============================================================================
    // Content
    
    juce::String dialogTitle;
    juce::String dialogMessage;
    ErrorType errorType;
    
    //==============================================================================
    // Styling
    
    void setupProfessionalStyling();
    juce::Colour getIconColor() const;
    juce::String getIconText() const;
    
    //==============================================================================
    // Event Handling
    
    void okButtonClicked();
    void detailsButtonClicked();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProfessionalErrorDialog)
};