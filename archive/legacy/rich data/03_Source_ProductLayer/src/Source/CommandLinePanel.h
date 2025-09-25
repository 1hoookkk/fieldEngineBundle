/******************************************************************************
 * File: CommandLinePanel.h
 * Description: CDP-style command line processing panel
 * 
 * REVOLUTIONARY CDP-INSPIRED COMMAND LINE INTERFACE
 * Professional terminal-style command processing like Composers Desktop Project
 * 
 * Copyright (c) 2025 Spectral Audio Systems
 ******************************************************************************/

#pragma once
#include <JuceHeader.h>
#include "RetroBrutalistLookAndFeel.h"

/**
 * @brief CDP-inspired command line processing panel
 * 
 * Revolutionary terminal-style interface for professional audio commands.
 * Inspired by Composers Desktop Project command line tools with authentic
 * terminal aesthetics and professional command processing.
 */
class CommandLinePanel : public juce::Component,
                        public juce::TextEditor::Listener,
                        public juce::Timer
{
public:
    CommandLinePanel();
    ~CommandLinePanel() override;
    
    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    // TextEditor::Listener overrides
    void textEditorReturnKeyPressed(juce::TextEditor& editor) override;
    void textEditorEscapeKeyPressed(juce::TextEditor& editor) override;
    
    // Timer for terminal cursor blinking
    void timerCallback() override;
    
    // Professional command processing
    void executeCommand(const juce::String& command);
    void addOutputLine(const juce::String& text, juce::Colour color = juce::Colours::green);
    void clearTerminal();
    void showHelp();
    
    // Terminal configuration
    void setPrompt(const juce::String& prompt) { terminalPrompt = prompt; }
    void setMaxHistoryLines(int maxLines) { maxHistoryLines = maxLines; }

private:
    //==============================================================================
    // Revolutionary Terminal Configuration
    
    juce::String terminalPrompt = "SCP> ";
    int maxHistoryLines = 500;
    bool cursorVisible = true;
    bool terminalEnabled = true;
    
    //==============================================================================
    // Terminal Components
    
    juce::TextEditor commandInput;
    juce::ListBox outputDisplay;
    
    //==============================================================================
    // Command History and Output
    
    juce::StringArray commandHistory;
    int historyIndex = -1;
    
    struct OutputLine
    {
        juce::String text;
        juce::Colour color;
        juce::Time timestamp;
        
        OutputLine(const juce::String& t, juce::Colour c) 
            : text(t), color(c), timestamp(juce::Time::getCurrentTime()) {}
    };
    
    std::vector<OutputLine> outputLines;
    
    //==============================================================================
    // Revolutionary CDP-Style Commands
    
    struct CommandInfo
    {
        juce::String name;
        juce::String description;
        juce::String usage;
        std::function<void(const juce::StringArray&)> handler;
    };
    
    std::map<juce::String, CommandInfo> commands;
    
    //==============================================================================
    // Terminal Visual Styling
    
    struct TerminalColors
    {
        static constexpr juce::uint32 backgroundBlack = 0xFF000000;     // Pure black
        static constexpr juce::uint32 textGreen = 0xFF00FF00;          // Terminal green
        static constexpr juce::uint32 textWhite = 0xFFFFFFFF;          // White text
        static constexpr juce::uint32 textYellow = 0xFFFFFF00;         // Warning yellow
        static constexpr juce::uint32 textRed = 0xFFFF0000;            // Error red
        static constexpr juce::uint32 textCyan = 0xFF00FFFF;           // Info cyan
        static constexpr juce::uint32 promptColor = 0xFF00AAAA;        // Prompt cyan
        static constexpr juce::uint32 cursorColor = 0xFFFFFFFF;        // White cursor
        static constexpr juce::uint32 borderColor = 0xFF333333;        // Dark border
    };
    
    //==============================================================================
    // Command Processing Implementation
    
    void initializeCommands();
    void processCommand(const juce::String& command);
    void addToHistory(const juce::String& command);
    void navigateHistory(int direction);
    juce::StringArray parseCommand(const juce::String& input);
    
    //==============================================================================
    // CDP-Style Command Handlers
    
    void handleHelpCommand(const juce::StringArray& args);
    void handleClearCommand(const juce::StringArray& args);
    void handleStatusCommand(const juce::StringArray& args);
    void handleLoadCommand(const juce::StringArray& args);
    void handlePlayCommand(const juce::StringArray& args);
    void handleStopCommand(const juce::StringArray& args);
    void handleChannelsCommand(const juce::StringArray& args);
    void handleOctaveCommand(const juce::StringArray& args);
    void handleStepCommand(const juce::StringArray& args);
    void handlePatternCommand(const juce::StringArray& args);
    void handleSynthCommand(const juce::StringArray& args);
    void handleSpecCommand(const juce::StringArray& args); // Spectral processing
    void handleMixCommand(const juce::StringArray& args);
    void handleExportCommand(const juce::StringArray& args);
    void handleVersionCommand(const juce::StringArray& args);
    
    //==============================================================================
    // Professional Output Formatting
    
    void printBanner();
    void printPrompt();
    void formatOutput(const juce::String& text, juce::Colour color = juce::Colours::green);
    void printError(const juce::String& error);
    void printWarning(const juce::String& warning);
    void printInfo(const juce::String& info);
    void printSuccess(const juce::String& success);
    
    //==============================================================================
    // ListBox Model for Output Display
    
    class TerminalListBoxModel : public juce::ListBoxModel
    {
    public:
        TerminalListBoxModel(CommandLinePanel* parent) : owner(parent) {}
        
        int getNumRows() override { return static_cast<int>(owner->outputLines.size()); }
        
        void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override
        {
            if (rowNumber >= 0 && rowNumber < static_cast<int>(owner->outputLines.size()))
            {
                const auto& line = owner->outputLines[rowNumber];
                
                g.setColour(line.color);
                g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain));
                g.drawText(line.text, 5, 0, width - 10, height, juce::Justification::centredLeft, false);
            }
        }
        
        CommandLinePanel* owner;
    };
    
    std::unique_ptr<TerminalListBoxModel> listBoxModel;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CommandLinePanel)
};