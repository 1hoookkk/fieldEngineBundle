#pragma once
#include <JuceHeader.h>
#include "MainComponent.h"
#include "Core/Config.h"

class SpectralCanvasApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "SPECTRAL CANVAS PRO"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }
    void initialise(const juce::String& commandLineParameters) override { 
        // Parse command line arguments for safety modes
        SpectralCanvas::Config::InitializeFromCommandLine(commandLineParameters.toRawUTF8());
        
        DBG("SpectralCanvasApp: " << SpectralCanvas::Config::GetConfigurationSummary());
        
        mainWindow.reset(new MainWindow(getApplicationName())); 
    }
    void shutdown() override { mainWindow = nullptr; }

private:
    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow(juce::String name)
            : DocumentWindow(name, juce::Colour(0xff1a1a1a), DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            
            // 🚨 SEGFAULT INVESTIGATION: Test MainComponent creation systematically
            try 
            {
                setContentOwned(new MainComponent(), true);
                DBG("🔍 SEGFAULT TEST: MainComponent created successfully");
            }
            catch (const std::exception& e)
            {
                DBG("🚨 SEGFAULT: Exception creating MainComponent: " << e.what());
                // Fallback to minimal component
                setContentOwned(new juce::Component(), true);
            }
            catch (...)
            {
                DBG("🚨 SEGFAULT: Unknown exception creating MainComponent");
                // Fallback to minimal component
                setContentOwned(new juce::Component(), true);
            }
            
            setResizable(true, true);
            centreWithSize(1200, 800);  // Perfect size for our revolutionary interface
            setVisible(true);
        }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };

    std::unique_ptr<MainWindow> mainWindow;
};
