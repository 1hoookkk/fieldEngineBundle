/**
 * Premium Feature Gating System Implementation
 * Controls access to advanced EMU rompler features
 */

#include "PremiumFeatures.h"
#include <algorithm>

//=============================================================================
// PremiumLicenseManager Implementation

PremiumLicenseManager::PremiumLicenseManager()
{
    loadLicenseData();
}

bool PremiumLicenseManager::isFeatureEnabled(PremiumFeature feature) const
{
    FeatureTier tier = currentTier.load();
    
    // If trial is active, use trial tier
    if (trialActive.load() && trialTier > tier)
    {
        // Check if trial is still valid
        if (isTrialActive())
        {
            tier = trialTier;
        }
    }
    
    return isFeatureInTier(feature, tier);
}

bool PremiumLicenseManager::validateLicense(const juce::String& licenseKey)
{
    // Simple license validation (in production, this would be more robust)
    if (licenseKey.isEmpty())
    {
        licenseValid.store(false);
        return false;
    }
    
    // Check license format and validate
    if (licenseKey.startsWith("SPECTRAL-") && licenseKey.length() >= 20)
    {
        currentLicenseKey = licenseKey;
        
        // Determine tier from license key
        if (licenseKey.contains("ELITE"))
        {
            setFeatureTier(FeatureTier::Elite);
        }
        else if (licenseKey.contains("PRO"))
        {
            setFeatureTier(FeatureTier::Professional);
        }
        else if (licenseKey.contains("STD"))
        {
            setFeatureTier(FeatureTier::Standard);
        }
        else
        {
            setFeatureTier(FeatureTier::Free);
        }
        
        licenseValid.store(true);
        saveLicenseData();
        return true;
    }
    
    licenseValid.store(false);
    return false;
}

void PremiumLicenseManager::setFeatureTier(FeatureTier tier)
{
    currentTier.store(tier);
}

juce::String PremiumLicenseManager::getFeatureDescription(PremiumFeature feature) const
{
    switch (feature)
    {
        case PremiumFeature::BasicPaintToAudio:
            return "Basic paint-to-audio synthesis";
        case PremiumFeature::SimpleSamplePlayback:
            return "Simple sample playback";
        case PremiumFeature::BasicFilter:
            return "Basic lowpass filter";
            
        case PremiumFeature::AdvancedSampleEngine:
            return "Advanced multi-sample engine with velocity layers";
        case PremiumFeature::EMUAudityFilter:
            return "EMU Audity-inspired multimode filter with resonance";
        case PremiumFeature::MultiLevelEnvelopes:
            return "Professional ADSR envelopes with curve shaping";
        case PremiumFeature::AdvancedLFOs:
            return "Multiple LFOs with sync and vintage character";
        case PremiumFeature::ArpeggiatorEngine:
            return "Built-in arpeggiator with classic patterns";
            
        case PremiumFeature::SpectralProcessing:
            return "Advanced spectral processing and analysis";
        case PremiumFeature::CDPInspiredEffects:
            return "CDP-inspired spectral transformation effects";
        case PremiumFeature::DualFilterMode:
            return "Dual filter processing in series/parallel";
        case PremiumFeature::ModulationMatrix:
            return "16-slot modulation matrix";
        case PremiumFeature::AdvancedExport:
            return "High-quality export with multiple formats";
        case PremiumFeature::PremiumPresets:
            return "Exclusive professional presets";
            
        case PremiumFeature::AIGeneratedContent:
            return "AI-generated samples and presets";
        case PremiumFeature::ExclusiveSamples:
            return "Exclusive sample library";
        case PremiumFeature::CustomWavetables:
            return "Custom wavetable editor";
        case PremiumFeature::ProfessionalIntegration:
            return "DAW integration and automation";
        case PremiumFeature::PrioritySupport:
            return "Priority customer support";
            
        default:
            return "Unknown feature";
    }
}

bool PremiumLicenseManager::requiresUpgrade(PremiumFeature feature) const
{
    return !isFeatureEnabled(feature);
}

juce::String PremiumLicenseManager::getUpgradeMessage(PremiumFeature feature) const
{
    if (isFeatureEnabled(feature))
        return juce::String();
        
    FeatureTier requiredTier = FeatureTier::Elite;
    
    // Determine minimum required tier
    if (isFeatureInTier(feature, FeatureTier::Standard))
        requiredTier = FeatureTier::Standard;
    else if (isFeatureInTier(feature, FeatureTier::Professional))
        requiredTier = FeatureTier::Professional;
    
    juce::String tierName;
    switch (requiredTier)
    {
        case FeatureTier::Standard:
            tierName = "Standard";
            break;
        case FeatureTier::Professional:
            tierName = "Professional";
            break;
        case FeatureTier::Elite:
            tierName = "Elite";
            break;
        default:
            tierName = "Premium";
            break;
    }
    
    return "This feature requires SpectralCanvas " + tierName + " edition. Upgrade now to unlock " + getFeatureDescription(feature) + ".";
}

void PremiumLicenseManager::startTrial(FeatureTier trialTier_, int durationDays)
{
    trialTier = trialTier_;
    trialDurationDays = durationDays;
    trialStartTime = juce::Time::getCurrentTime();
    trialActive.store(true);
    
    saveLicenseData();
}

bool PremiumLicenseManager::isTrialActive() const
{
    if (!trialActive.load())
        return false;
        
    auto currentTime = juce::Time::getCurrentTime();
    auto elapsedDays = (currentTime - trialStartTime).inDays();
    
    return elapsedDays < trialDurationDays;
}

int PremiumLicenseManager::getTrialDaysRemaining() const
{
    if (!isTrialActive())
        return 0;
        
    auto currentTime = juce::Time::getCurrentTime();
    auto elapsedDays = (currentTime - trialStartTime).inDays();
    
    return juce::jmax(0, trialDurationDays - (int)elapsedDays);
}

void PremiumLicenseManager::endTrial()
{
    trialActive.store(false);
    saveLicenseData();
}

void PremiumLicenseManager::trackFeatureUsage(PremiumFeature feature)
{
    // In a production app, this would log to analytics
    (void)feature;
}

void PremiumLicenseManager::trackPaintStroke()
{
    paintStrokeCount.fetch_add(1);
}

void PremiumLicenseManager::trackSampleLoad()
{
    sampleLoadCount.fetch_add(1);
}

void PremiumLicenseManager::trackExport()
{
    exportCount.fetch_add(1);
}

PremiumLicenseManager::LicenseStatus PremiumLicenseManager::getLicenseStatus() const
{
    LicenseStatus status;
    status.tier = currentTier.load();
    status.isValid = licenseValid.load();
    status.isTrialActive = isTrialActive();
    status.trialDaysRemaining = getTrialDaysRemaining();
    status.licenseName = licensedUserName;
    status.expiryDate = "Never"; // Could be extended for subscription model
    
    return status;
}

bool PremiumLicenseManager::isFeatureInTier(PremiumFeature feature, FeatureTier tier) const
{
    switch (feature)
    {
        // Free tier features
        case PremiumFeature::BasicPaintToAudio:
        case PremiumFeature::SimpleSamplePlayback:
        case PremiumFeature::BasicFilter:
            return tier >= FeatureTier::Free;
            
        // Standard tier features
        case PremiumFeature::AdvancedSampleEngine:
        case PremiumFeature::EMUAudityFilter:
        case PremiumFeature::MultiLevelEnvelopes:
        case PremiumFeature::AdvancedLFOs:
        case PremiumFeature::ArpeggiatorEngine:
            return tier >= FeatureTier::Standard;
            
        // Professional tier features
        case PremiumFeature::SpectralProcessing:
        case PremiumFeature::CDPInspiredEffects:
        case PremiumFeature::DualFilterMode:
        case PremiumFeature::ModulationMatrix:
        case PremiumFeature::AdvancedExport:
        case PremiumFeature::PremiumPresets:
            return tier >= FeatureTier::Professional;
            
        // Elite tier features
        case PremiumFeature::AIGeneratedContent:
        case PremiumFeature::ExclusiveSamples:
        case PremiumFeature::CustomWavetables:
        case PremiumFeature::ProfessionalIntegration:
        case PremiumFeature::PrioritySupport:
            return tier >= FeatureTier::Elite;
            
        default:
            return false;
    }
}

void PremiumLicenseManager::saveLicenseData()
{
    // Save license data to application properties
    juce::PropertiesFile::Options options;
    options.applicationName = "SpectralCanvas";
    options.filenameSuffix = ".settings";
    options.osxLibrarySubFolder = "Application Support";
    
    auto properties = std::make_unique<juce::PropertiesFile>(options);
    
    properties->setValue("currentTier", (int)currentTier.load());
    properties->setValue("licenseValid", licenseValid.load());
    properties->setValue("currentLicenseKey", currentLicenseKey);
    properties->setValue("licensedUserName", licensedUserName);
    
    if (trialActive.load())
    {
        properties->setValue("trialActive", true);
        properties->setValue("trialTier", (int)trialTier);
        properties->setValue("trialDurationDays", trialDurationDays);
        properties->setValue("trialStartTime", trialStartTime.toMilliseconds());
    }
    
    properties->saveIfNeeded();
}

void PremiumLicenseManager::loadLicenseData()
{
    juce::PropertiesFile::Options options;
    options.applicationName = "SpectralCanvas";
    options.filenameSuffix = ".settings";
    options.osxLibrarySubFolder = "Application Support";
    
    auto properties = std::make_unique<juce::PropertiesFile>(options);
    
    currentTier.store((FeatureTier)properties->getIntValue("currentTier", (int)FeatureTier::Free));
    licenseValid.store(properties->getBoolValue("licenseValid", false));
    currentLicenseKey = properties->getValue("currentLicenseKey", "");
    licensedUserName = properties->getValue("licensedUserName", "");
    
    if (properties->getBoolValue("trialActive", false))
    {
        trialActive.store(true);
        trialTier = (FeatureTier)properties->getIntValue("trialTier", (int)FeatureTier::Free);
        trialDurationDays = properties->getIntValue("trialDurationDays", 0);
        trialStartTime = juce::Time(properties->getDoubleValue("trialStartTime", 0));
        
        // Check if trial is still valid
        if (!isTrialActive())
        {
            trialActive.store(false);
        }
    }
}

//=============================================================================
// FeatureGateComponent Implementation

FeatureGateComponent::FeatureGateComponent(PremiumFeature requiredFeature_)
    : requiredFeature(requiredFeature_)
{
}

void FeatureGateComponent::paint(juce::Graphics& g)
{
    if (!PremiumFeatures::isEnabled(requiredFeature))
    {
        // Draw locked overlay
        g.fillAll(juce::Colours::black.withAlpha(0.7f));
        
        g.setColour(juce::Colours::orange);
        g.setFont(16.0f);
        
        auto bounds = getLocalBounds();
        g.drawText("🔒 Premium Feature", bounds.removeFromTop(30), juce::Justification::centred, true);
        
        g.setColour(juce::Colours::white);
        g.setFont(12.0f);
        g.drawText("Click to upgrade", bounds, juce::Justification::centred, true);
    }
    else if (protectedComponent)
    {
        // Feature is available, show the protected component
        protectedComponent->setBounds(getLocalBounds());
    }
}

void FeatureGateComponent::mouseDown(const juce::MouseEvent& event)
{
    if (!PremiumFeatures::isEnabled(requiredFeature))
    {
        showUpgradePrompt();
    }
}

void FeatureGateComponent::setProtectedComponent(std::unique_ptr<juce::Component> component)
{
    protectedComponent = std::move(component);
    
    if (protectedComponent && PremiumFeatures::isEnabled(requiredFeature))
    {
        addAndMakeVisible(protectedComponent.get());
        protectedComponent->setBounds(getLocalBounds());
    }
}

void FeatureGateComponent::showUpgradePrompt()
{
    showUpgradeDialog(requiredFeature);
}

//=============================================================================
// PremiumContentManager Implementation

PremiumContentManager::PremiumContentManager()
{
    // Set up content directory
    contentDirectory = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                          .getChildFile("SpectralCanvas")
                          .getChildFile("PremiumContent");
                          
    if (!contentDirectory.exists())
    {
        contentDirectory.createDirectory();
    }
    
    refreshContentLibrary();
}

std::vector<juce::File> PremiumContentManager::getAvailableContent(ContentType type) const
{
    switch (type)
    {
        case ContentType::Samples:
            return sampleLibrary;
        case ContentType::Presets:
            return presetLibrary;
        case ContentType::Wavetables:
            return wavetableLibrary;
        default:
            return {};
    }
}

bool PremiumContentManager::isContentAvailable(const juce::File& contentFile) const
{
    // Check if content requires premium features
    juce::String filename = contentFile.getFileName();
    
    if (filename.contains("Premium") || filename.contains("Elite"))
    {
        return PremiumFeatures::isEnabled(PremiumFeature::PremiumPresets);
    }
    
    if (filename.contains("AI") || filename.contains("Generated"))
    {
        return PremiumFeatures::isEnabled(PremiumFeature::AIGeneratedContent);
    }
    
    if (filename.contains("Exclusive"))
    {
        return PremiumFeatures::isEnabled(PremiumFeature::ExclusiveSamples);
    }
    
    return true; // Free content
}

bool PremiumContentManager::downloadPremiumContent(ContentType type)
{
    // In a real implementation, this would download from a server
    (void)type;
    return false; // Not implemented in this demo
}

bool PremiumContentManager::generateAIContent(const AIContentRequest& request)
{
    if (!PremiumFeatures::isEnabled(PremiumFeature::AIGeneratedContent))
        return false;
        
    // In a real implementation, this would call an AI service
    (void)request;
    return false; // Not implemented in this demo
}

bool PremiumContentManager::isAIGenerationAvailable() const
{
    return PremiumFeatures::isEnabled(PremiumFeature::AIGeneratedContent);
}

void PremiumContentManager::refreshContentLibrary()
{
    sampleLibrary.clear();
    presetLibrary.clear();
    wavetableLibrary.clear();
    
    // Scan content directory
    auto samplesDir = contentDirectory.getChildFile("Samples");
    auto presetsDir = contentDirectory.getChildFile("Presets");
    auto wavetablesDir = contentDirectory.getChildFile("Wavetables");
    
    if (samplesDir.exists())
    {
        auto sampleFiles = samplesDir.findChildFiles(juce::File::findFiles, false, "*.wav;*.aif;*.flac", juce::File::FollowSymlinks::yes);
        for (const auto& file : sampleFiles)
        {
            sampleLibrary.push_back(file);
        }
    }
    
    if (presetsDir.exists())
    {
        auto presetFiles = presetsDir.findChildFiles(juce::File::findFiles, false, "*.preset", juce::File::FollowSymlinks::yes);
        for (const auto& file : presetFiles)
        {
            presetLibrary.push_back(file);
        }
    }
    
    if (wavetablesDir.exists())
    {
        auto wavetableFiles = wavetablesDir.findChildFiles(juce::File::findFiles, false, "*.wav;*.wt", juce::File::FollowSymlinks::yes);
        for (const auto& file : wavetableFiles)
        {
            wavetableLibrary.push_back(file);
        }
    }
}

void PremiumContentManager::clearCachedContent()
{
    // Clear cached content (but keep user's own content)
}

int PremiumContentManager::getTotalContentSize() const
{
    int totalSize = 0;
    
    for (const auto& file : sampleLibrary)
    {
        totalSize += (int)(file.getSize() / 1024 / 1024); // Convert to MB
    }
    
    for (const auto& file : presetLibrary)
    {
        totalSize += (int)(file.getSize() / 1024 / 1024);
    }
    
    for (const auto& file : wavetableLibrary)
    {
        totalSize += (int)(file.getSize() / 1024 / 1024);
    }
    
    return totalSize;
}

//=============================================================================
// PremiumFeatures Singleton Implementation

PremiumFeatures& PremiumFeatures::getInstance()
{
    static PremiumFeatures instance;
    return instance;
}

bool PremiumFeatures::isEnabled(PremiumFeature feature)
{
    return getInstance().getLicenseManager().isFeatureEnabled(feature);
}

FeatureTier PremiumFeatures::getCurrentTier()
{
    return getInstance().getLicenseManager().getCurrentTier();
}

bool PremiumFeatures::requiresUpgrade(PremiumFeature feature)
{
    return getInstance().getLicenseManager().requiresUpgrade(feature);
}

bool PremiumFeatures::validateLicense(const juce::String& key)
{
    return getInstance().getLicenseManager().validateLicense(key);
}

void PremiumFeatures::setTier(FeatureTier tier)
{
    getInstance().getLicenseManager().setFeatureTier(tier);
}

void PremiumFeatures::startTrial(FeatureTier tier, int days)
{
    getInstance().getLicenseManager().startTrial(tier, days);
}

void PremiumFeatures::trackPaintStroke()
{
    getInstance().getLicenseManager().trackPaintStroke();
}

void PremiumFeatures::trackSampleLoad()
{
    getInstance().getLicenseManager().trackSampleLoad();
}

void PremiumFeatures::trackExport()
{
    getInstance().getLicenseManager().trackExport();
}

void PremiumFeatures::trackFeatureUse(PremiumFeature feature)
{
    getInstance().getLicenseManager().trackFeatureUsage(feature);
}

PremiumContentManager& PremiumFeatures::getContentManager()
{
    auto& instance = getInstance();
    if (!instance.contentManager)
    {
        instance.contentManager = std::make_unique<PremiumContentManager>();
    }
    return *instance.contentManager;
}

PremiumLicenseManager& PremiumFeatures::getLicenseManager()
{
    auto& instance = getInstance();
    if (!instance.licenseManager)
    {
        instance.licenseManager = std::make_unique<PremiumLicenseManager>();
    }
    return *instance.licenseManager;
}

//=============================================================================
// Premium UI Dialog Functions

void showUpgradeDialog(PremiumFeature feature)
{
    auto& licenseManager = PremiumFeatures::getLicenseManager();
    juce::String message = licenseManager.getUpgradeMessage(feature);
    
    juce::AlertWindow::showMessageBoxAsync(
        juce::AlertWindow::InfoIcon,
        "Premium Feature Required",
        message + "\n\nWould you like to start a free trial or purchase a license?",
        "OK"
    );
}

void showFeatureLockedMessage(PremiumFeature feature)
{
    auto& licenseManager = PremiumFeatures::getLicenseManager();
    juce::String featureName = licenseManager.getFeatureDescription(feature);
    
    juce::AlertWindow::showMessageBoxAsync(
        juce::AlertWindow::WarningIcon,
        "Feature Locked",
        featureName + " is not available in your current license tier."
    );
}

void showTrialDialog(FeatureTier suggestedTier)
{
    juce::String tierName;
    switch (suggestedTier)
    {
        case FeatureTier::Professional:
            tierName = "Professional";
            break;
        case FeatureTier::Elite:
            tierName = "Elite";
            break;
        default:
            tierName = "Premium";
            break;
    }
    
    juce::AlertWindow::showMessageBoxAsync(
        juce::AlertWindow::InfoIcon,
        "Start Free Trial",
        "Try SpectralCanvas " + tierName + " edition free for 14 days!\n\nFull access to all features with no limitations."
    );
}

void showLicenseDialog()
{
    // For now, just show a simple message - in production this would be a proper dialog
    juce::AlertWindow::showMessageBoxAsync(
        juce::AlertWindow::InfoIcon,
        "License Key Required",
        "License validation will be implemented in the final release.\n\nFor now, features are unlocked by default in development mode."
    );
}

//=============================================================================
// Premium Feature System Initialization

void initializePremiumFeatures()
{
    // Initialize the singleton and load license data
    PremiumFeatures::getInstance();
}

void shutdownPremiumFeatures()
{
    // Cleanup is handled by singleton destructor
}