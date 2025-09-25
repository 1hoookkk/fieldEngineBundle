/******************************************************************************
 * File: ProfessionalErrorDialog.cpp
 * Description: Implementation of professional error dialog
 * 
 * Vintage DAW-style error reporting with authentic professional aesthetics.
 * 
 * Copyright (c) 2025 Spectral Audio Systems
 ******************************************************************************/

#include "ProfessionalErrorDialog.h"

//==============================================================================
// Constructor
//==============================================================================

ProfessionalErrorDialog::ProfessionalErrorDialog(const juce::String& title, 
                                                 const juce::String& message,
                                                 ErrorType type)
    : dialogTitle(title), dialogMessage(message), errorType(type)
{
    setupProfessionalStyling();
    
    // Set dialog size
    setSize(450, 200);
}

//==============================================================================
// Static Display Methods
//==============================================================================

void ProfessionalErrorDialog::showError(const juce::String& title, const juce::String& message)
{
    auto dialog = std::make_unique<ProfessionalErrorDialog>(title, message, ErrorType::SystemError);
    
    juce::DialogWindow::LaunchOptions options;
    options.content.setNonOwned(dialog.release());
    options.dialogTitle = "SPECTRAL CANVAS PRO - ERROR";
    options.dialogBackgroundColour = juce::Colour(VintageProLookAndFeel::VintageColors::backgroundDark);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = false;
    options.resizable = false;
    options.useBottomRightCornerResizer = false;
    
    options.launchAsync();
}

void ProfessionalErrorDialog::showFileError(const juce::String& fileName, const juce::String& details)
{
    juce::String title = "FILE LOAD ERROR";
    juce::String message = "Failed to load: " + fileName + "\n\n" + details;
    
    auto dialog = std::make_unique<ProfessionalErrorDialog>(title, message, ErrorType::FileError);
    
    juce::DialogWindow::LaunchOptions options;
    options.content.setNonOwned(dialog.release());
    options.dialogTitle = "SPECTRAL CANVAS PRO - FILE ERROR";
    options.dialogBackgroundColour = juce::Colour(VintageProLookAndFeel::VintageColors::backgroundDark);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = false;
    options.resizable = false;
    
    options.launchAsync();
}

void ProfessionalErrorDialog::showWarning(const juce::String& title, const juce::String& message)
{
    auto dialog = std::make_unique<ProfessionalErrorDialog>(title, message, ErrorType::WarningMessage);
    
    juce::DialogWindow::LaunchOptions options;
    options.content.setNonOwned(dialog.release());
    options.dialogTitle = "SPECTRAL CANVAS PRO - WARNING";
    options.dialogBackgroundColour = juce::Colour(VintageProLookAndFeel::VintageColors::backgroundDark);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = false;
    options.resizable = false;
    
    options.launchAsync();
}

void ProfessionalErrorDialog::showInfo(const juce::String& title, const juce::String& message)
{
    auto dialog = std::make_unique<ProfessionalErrorDialog>(title, message, ErrorType::InfoMessage);
    
    juce::DialogWindow::LaunchOptions options;
    options.content.setNonOwned(dialog.release());
    options.dialogTitle = "SPECTRAL CANVAS PRO - INFORMATION";
    options.dialogBackgroundColour = juce::Colour(VintageProLookAndFeel::VintageColors::backgroundDark);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = false;
    options.resizable = false;
    
    options.launchAsync();
}

//==============================================================================
// Component Interface
//==============================================================================

void ProfessionalErrorDialog::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    
    // Professional dialog background
    g.fillAll(juce::Colour(VintageProLookAndFeel::VintageColors::backgroundDark));
    
    // Dialog border
    g.setColour(juce::Colour(VintageProLookAndFeel::VintageColors::panelMedium));
    g.drawRect(bounds, 2);
    
    // Icon area
    auto iconArea = bounds.removeFromLeft(60).reduced(10);
    iconArea = iconArea.removeFromTop(50);
    
    // Draw professional error icon
    g.setColour(getIconColor());
    g.setFont(juce::Font(24.0f, juce::Font::FontStyleFlags::bold));
    g.drawText(getIconText(), iconArea, juce::Justification::centred);
    
    // Icon background circle
    g.setColour(getIconColor().withAlpha(0.2f));
    g.fillEllipse(iconArea.expanded(5).toFloat());
    g.setColour(getIconColor());
    g.drawEllipse(iconArea.expanded(5).toFloat(), 2.0f);
}

void ProfessionalErrorDialog::resized()
{
    auto bounds = getLocalBounds().reduced(10);
    
    // Leave space for icon
    bounds.removeFromLeft(60);
    
    // Title area
    auto titleArea = bounds.removeFromTop(30);
    titleLabel.setBounds(titleArea);
    
    bounds.removeFromTop(10); // Spacing
    
    // Button area
    auto buttonArea = bounds.removeFromBottom(35);
    okButton.setBounds(buttonArea.removeFromRight(80));
    buttonArea.removeFromRight(10); // Spacing
    detailsButton.setBounds(buttonArea.removeFromRight(80));
    
    bounds.removeFromBottom(10); // Spacing
    
    // Message area
    messageEditor.setBounds(bounds);
}

//==============================================================================
// Professional Styling
//==============================================================================

void ProfessionalErrorDialog::setupProfessionalStyling()
{
    // Title label
    addAndMakeVisible(titleLabel);
    titleLabel.setText(dialogTitle, juce::dontSendNotification);
    titleLabel.setFont(juce::Font(14.0f, juce::Font::FontStyleFlags::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colour(VintageProLookAndFeel::VintageColors::textPrimary));
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    
    // Message editor
    addAndMakeVisible(messageEditor);
    messageEditor.setText(dialogMessage);
    messageEditor.setReadOnly(true);
    messageEditor.setMultiLine(true);
    messageEditor.setScrollbarsShown(true);
    messageEditor.setFont(juce::Font(11.0f));
    messageEditor.setColour(juce::TextEditor::backgroundColourId, 
                           juce::Colour(VintageProLookAndFeel::VintageColors::panelMedium));
    messageEditor.setColour(juce::TextEditor::textColourId, 
                           juce::Colour(VintageProLookAndFeel::VintageColors::textPrimary));
    messageEditor.setColour(juce::TextEditor::outlineColourId, 
                           juce::Colour(VintageProLookAndFeel::VintageColors::panelLight));
    
    // OK button
    addAndMakeVisible(okButton);
    okButton.setButtonText("OK");
    okButton.setColour(juce::TextButton::buttonColourId, 
                      juce::Colour(VintageProLookAndFeel::VintageColors::panelMedium));
    okButton.setColour(juce::TextButton::textColourOffId, 
                      juce::Colour(VintageProLookAndFeel::VintageColors::textPrimary));
    okButton.onClick = [this]() { okButtonClicked(); };
    
    // Details button
    addAndMakeVisible(detailsButton);
    detailsButton.setButtonText("DETAILS");
    detailsButton.setColour(juce::TextButton::buttonColourId, 
                           juce::Colour(VintageProLookAndFeel::VintageColors::panelMedium));
    detailsButton.setColour(juce::TextButton::textColourOffId, 
                           juce::Colour(VintageProLookAndFeel::VintageColors::textPrimary));
    detailsButton.onClick = [this]() { detailsButtonClicked(); };
}

juce::Colour ProfessionalErrorDialog::getIconColor() const
{
    switch (errorType)
    {
        case ErrorType::FileError:
        case ErrorType::SystemError:
            return juce::Colour(VintageProLookAndFeel::VintageColors::meterRed);
        case ErrorType::WarningMessage:
            return juce::Colour(VintageProLookAndFeel::VintageColors::meterAmber);
        case ErrorType::InfoMessage:
            return juce::Colour(VintageProLookAndFeel::VintageColors::accentBlue);
        default:
            return juce::Colour(VintageProLookAndFeel::VintageColors::meterRed);
    }
}

juce::String ProfessionalErrorDialog::getIconText() const
{
    switch (errorType)
    {
        case ErrorType::FileError:
        case ErrorType::SystemError:
            return "!";
        case ErrorType::WarningMessage:
            return "⚠";
        case ErrorType::InfoMessage:
            return "i";
        default:
            return "!";
    }
}

//==============================================================================
// Event Handling
//==============================================================================

void ProfessionalErrorDialog::okButtonClicked()
{
    if (auto* window = findParentComponentOfClass<juce::DialogWindow>())
    {
        window->exitModalState(1);
    }
}

void ProfessionalErrorDialog::detailsButtonClicked()
{
    // Show additional technical details
    juce::String details = "SPECTRAL CANVAS PRO ERROR DETAILS\n";
    details += "=====================================\n\n";
    juce::String errorTypeText = 
        (errorType == ErrorType::FileError ? "File Error" :
         errorType == ErrorType::SystemError ? "System Error" :
         errorType == ErrorType::WarningMessage ? "Warning" : "Information");
    details += "Error Type: " + errorTypeText + "\n";
    details += "Time: " + juce::Time::getCurrentTime().toString(true, true) + "\n";
    details += "Version: 1.0.0\n\n";
    details += "Message:\n" + dialogMessage + "\n\n";
    details += "Troubleshooting:\n";
    
    if (errorType == ErrorType::FileError)
    {
        details += "• Check file format (supported: WAV, AIFF, MP3, FLAC, OGG)\n";
        details += "• Verify file is not corrupted\n";
        details += "• Ensure file permissions allow reading\n";
        details += "• Try converting to WAV format\n";
    }
    else
    {
        details += "• Restart the application\n";
        details += "• Check available system memory\n";
        details += "• Verify audio device settings\n";
    }
    
    showInfo("Technical Details", details);
}