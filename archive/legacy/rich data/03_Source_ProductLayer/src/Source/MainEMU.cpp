/**
 * SpectralCanvas EMU Rompler Main Application
 * Launches the EMU Rompler version with AI-generated visuals support
 * Built on the proven clean foundation
 */

#include <JuceHeader.h>
#include "EMURomplerComponent.h"
#include "Core/PremiumFeatures.h"
#include "UI/EMUAssetManager.h"
#include "UI/EMUAudityLookAndFeel.h"

// Professional EMU styling instance
static std::unique_ptr<EMUAudityLookAndFeel> professionalEMUStyle;

/**
 * Main Application Window for EMU Rompler
 */
class EMURomplerWindow : public juce::DocumentWindow
{
public:
    EMURomplerWindow(juce::String name) : DocumentWindow(name,
                                                         juce::Colour(30, 58, 95), // EMU metallic blue background
                                                         DocumentWindow::allButtons)
    {
        // 🎹 Apply professional EMU Audity styling
        if (!professionalEMUStyle)
            professionalEMUStyle = std::make_unique<EMUAudityLookAndFeel>();
        setLookAndFeel(professionalEMUStyle.get());
        
        setUsingNativeTitleBar(true);
        setContentOwned(new EMURomplerComponent(), true);
        
        // Set professional desktop interface size (1400x900)
        setSize(1400, 900);
        
        // Center on screen
        centreWithSize(getWidth(), getHeight());
        
        setVisible(true);
        setResizable(true, true);
        
        // Set size limits for professional desktop interface
        setResizeLimits(1400, 900, 2800, 1800); // Maintain aspect ratio and EMU layout integrity
    }
    
    ~EMURomplerWindow() override
    {
        // Clean up professional styling
        setLookAndFeel(nullptr);
    }
    
    void closeButtonPressed() override
    {
        JUCEApplication::getInstance()->systemRequestedQuit();
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EMURomplerWindow)
};

/**
 * SpectralCanvas EMU Rompler Application
 */
class SpectralCanvasEMUApp : public juce::JUCEApplication
{
public:
    SpectralCanvasEMUApp() {}

    const juce::String getApplicationName() override       { return ProjectInfo::projectName; }
    const juce::String getApplicationVersion() override    { return ProjectInfo::versionString; }
    bool moreThanOneInstanceAllowed() override             { return true; }

    void initialise(const juce::String& commandLine) override
    {
        // Initialize premium features system
        initializePremiumFeatures();
        
        // Initialize EMU asset management system
        initializeEMUAssets();
        
        // Set default feature tier (Free for initial distribution)
        PremiumFeatures::setTier(FeatureTier::Free);
        
        // Check for command line arguments
        if (commandLine.contains("--premium"))
            PremiumFeatures::setTier(FeatureTier::Professional);
        if (commandLine.contains("--elite"))
            PremiumFeatures::setTier(FeatureTier::Elite);
        if (commandLine.contains("--trial"))
            PremiumFeatures::startTrial(FeatureTier::Professional, 14);
            
        // Create main window
        mainWindow.reset(new EMURomplerWindow(getApplicationName() + " - EMU Rompler"));
        
        // Show welcome message based on tier
        showWelcomeMessage();
    }

    void shutdown() override
    {
        mainWindow = nullptr;
        shutdownEMUAssets();
        shutdownPremiumFeatures();
        
        // Clean up professional EMU styling
        professionalEMUStyle = nullptr;
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted(const juce::String& commandLine) override
    {
        // Handle multiple instances if needed
    }
    
private:
    std::unique_ptr<EMURomplerWindow> mainWindow;
    
    void showWelcomeMessage()
    {
        auto tier = PremiumFeatures::getCurrentTier();
        juce::String message;
        
        switch (tier)
        {
            case FeatureTier::Free:
                message = "Welcome to SpectralCanvas EMU Rompler (Free Edition)!\n\n"
                         "• Paint-to-audio synthesis\n"
                         "• Basic EMU rompler features\n"
                         "• 8 sample slots\n"
                         "• EMU Audity-style filter\n\n"
                         "Upgrade for advanced spectral processing and exclusive content.";
                break;
                
            case FeatureTier::Standard:
                message = "Welcome to SpectralCanvas EMU Rompler (Standard Edition)!\n\n"
                         "• Full rompler features unlocked\n"
                         "• Advanced envelopes and LFOs\n"
                         "• Modulation matrix\n"
                         "• Premium presets\n\n"
                         "Thank you for supporting SpectralCanvas!";
                break;
                
            case FeatureTier::Professional:
                message = "Welcome to SpectralCanvas EMU Rompler (Professional Edition)!\n\n"
                         "• Advanced spectral processing\n"
                         "• CDP-inspired effects\n"
                         "• Dual filter modes\n"
                         "• Professional export options\n\n"
                         "You have access to professional-grade tools!";
                break;
                
            case FeatureTier::Elite:
                message = "Welcome to SpectralCanvas EMU Rompler (Elite Edition)!\n\n"
                         "• All features unlocked\n"
                         "• AI-generated content\n"
                         "• Exclusive sample libraries\n"
                         "• Priority support\n\n"
                         "Thank you for being an Elite member!";
                break;
        }
        
        if (PremiumFeatures::getInstance().getLicenseManager().isTrialActive())
        {
            int daysRemaining = PremiumFeatures::getInstance().getLicenseManager().getTrialDaysRemaining();
            message += "\n\n[TRIAL MODE - " + juce::String(daysRemaining) + " days remaining]";
        }
        
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::InfoIcon,
            "SpectralCanvas EMU Rompler",
            message
        );
    }
};

//=============================================================================
// Application startup

START_JUCE_APPLICATION(SpectralCanvasEMUApp)