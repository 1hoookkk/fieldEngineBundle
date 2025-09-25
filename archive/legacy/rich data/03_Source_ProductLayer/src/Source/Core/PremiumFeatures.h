/**
 * Premium Feature Gating System
 * Controls access to advanced EMU rompler features and exclusive content
 * Implements professional licensing and feature management
 */

#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <string>
#include <vector>
#include <memory>

/**
 * Feature tiers for SpectralCanvas EMU Rompler
 */
enum class FeatureTier
{
    Free = 0,           // Basic paint-to-audio + simple rompler
    Standard = 1,       // Full rompler features + basic spectral processing
    Professional = 2,   // Advanced spectral processing + exclusive content
    Elite = 3          // All features + custom samples + priority updates
};

/**
 * Individual premium features that can be gated
 */
enum class PremiumFeature
{
    // Core Features (always available in Free tier)
    BasicPaintToAudio,
    SimpleSamplePlayback,
    BasicFilter,
    
    // Standard Tier Features
    AdvancedSampleEngine,
    EMUAudityFilter,
    MultiLevelEnvelopes,
    AdvancedLFOs,
    ArpeggiatorEngine,
    
    // Professional Tier Features
    SpectralProcessing,
    CDPInspiredEffects,
    DualFilterMode,
    ModulationMatrix,
    AdvancedExport,
    PremiumPresets,
    
    // Elite Tier Features
    AIGeneratedContent,
    ExclusiveSamples,
    CustomWavetables,
    ProfessionalIntegration,
    PrioritySupport
};

/**
 * License verification and feature management
 */
class PremiumLicenseManager
{
public:
    PremiumLicenseManager();
    ~PremiumLicenseManager() = default;
    
    // License management
    bool isFeatureEnabled(PremiumFeature feature) const;
    FeatureTier getCurrentTier() const { return currentTier.load(); }
    bool validateLicense(const juce::String& licenseKey);
    void setFeatureTier(FeatureTier tier);
    
    // Feature status
    juce::String getFeatureDescription(PremiumFeature feature) const;
    bool requiresUpgrade(PremiumFeature feature) const;
    juce::String getUpgradeMessage(PremiumFeature feature) const;
    
    // Trial system
    void startTrial(FeatureTier trialTier, int durationDays);
    bool isTrialActive() const;
    int getTrialDaysRemaining() const;
    void endTrial();
    
    // Usage tracking (for analytics)
    void trackFeatureUsage(PremiumFeature feature);
    void trackPaintStroke();
    void trackSampleLoad();
    void trackExport();
    
    // License status for UI
    struct LicenseStatus
    {
        FeatureTier tier;
        bool isValid;
        bool isTrialActive;
        int trialDaysRemaining;
        juce::String licenseName;
        juce::String expiryDate;
    };
    LicenseStatus getLicenseStatus() const;
    
private:
    std::atomic<FeatureTier> currentTier{FeatureTier::Free};
    std::atomic<bool> licenseValid{false};
    std::atomic<bool> trialActive{false};
    
    juce::Time trialStartTime;
    int trialDurationDays = 0;
    FeatureTier trialTier = FeatureTier::Free;
    
    juce::String currentLicenseKey;
    juce::String licensedUserName;
    
    // Usage statistics
    std::atomic<int> paintStrokeCount{0};
    std::atomic<int> sampleLoadCount{0};
    std::atomic<int> exportCount{0};
    
    // Internal methods
    bool isFeatureInTier(PremiumFeature feature, FeatureTier tier) const;
    void saveLicenseData();
    void loadLicenseData();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PremiumLicenseManager)
};

/**
 * Feature gate decorator for UI components
 * Shows premium upgrade prompts when features are not available
 */
class FeatureGateComponent : public juce::Component
{
public:
    FeatureGateComponent(PremiumFeature requiredFeature);
    ~FeatureGateComponent() override = default;
    
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& event) override;
    
    // Set the component to protect
    void setProtectedComponent(std::unique_ptr<juce::Component> component);
    void showUpgradePrompt();
    
private:
    PremiumFeature requiredFeature;
    std::unique_ptr<juce::Component> protectedComponent;
    bool showingPrompt = false;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FeatureGateComponent)
};

/**
 * Premium content manager
 * Handles exclusive samples, presets, and AI-generated content
 */
class PremiumContentManager
{
public:
    PremiumContentManager();
    ~PremiumContentManager() = default;
    
    // Content categories
    enum class ContentType
    {
        Samples,
        Presets, 
        Wavetables,
        AIGeneratedContent,
        ExclusiveContent
    };
    
    // Content access
    std::vector<juce::File> getAvailableContent(ContentType type) const;
    bool isContentAvailable(const juce::File& contentFile) const;
    bool downloadPremiumContent(ContentType type);
    
    // AI content generation (Elite tier)
    struct AIContentRequest
    {
        juce::String description;
        ContentType type;
        juce::String style;
        int duration; // For samples
        int complexity; // 1-10
    };
    bool generateAIContent(const AIContentRequest& request);
    bool isAIGenerationAvailable() const;
    
    // Content library management
    void refreshContentLibrary();
    void clearCachedContent();
    int getTotalContentSize() const; // MB
    
private:
    juce::File contentDirectory;
    std::vector<juce::File> sampleLibrary;
    std::vector<juce::File> presetLibrary;
    std::vector<juce::File> wavetableLibrary;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PremiumContentManager)
};

/**
 * Global premium feature access
 * Singleton for easy access throughout the application
 */
class PremiumFeatures
{
public:
    static PremiumFeatures& getInstance();
    
    // Feature checking
    static bool isEnabled(PremiumFeature feature);
    static FeatureTier getCurrentTier();
    static bool requiresUpgrade(PremiumFeature feature);
    
    // License management
    static bool validateLicense(const juce::String& key);
    static void setTier(FeatureTier tier);
    static void startTrial(FeatureTier tier, int days);
    
    // Usage tracking
    static void trackPaintStroke();
    static void trackSampleLoad();
    static void trackExport();
    static void trackFeatureUse(PremiumFeature feature);
    
    // Content access
    static PremiumContentManager& getContentManager();
    static PremiumLicenseManager& getLicenseManager();
    
private:
    PremiumFeatures() = default;
    
    std::unique_ptr<PremiumLicenseManager> licenseManager;
    std::unique_ptr<PremiumContentManager> contentManager;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PremiumFeatures)
};

/**
 * Convenience macros for feature gating
 */
#define PREMIUM_FEATURE_REQUIRED(feature) \
    if (!PremiumFeatures::isEnabled(feature)) { \
        showUpgradeDialog(feature); \
        return; \
    }

#define PREMIUM_FEATURE_CHECK(feature, action) \
    if (PremiumFeatures::isEnabled(feature)) { \
        action; \
    } else { \
        showFeatureLockedMessage(feature); \
    }

/**
 * Premium UI dialogs
 */
void showUpgradeDialog(PremiumFeature feature);
void showFeatureLockedMessage(PremiumFeature feature);
void showTrialDialog(FeatureTier suggestedTier);
void showLicenseDialog();

/**
 * Premium feature initialization
 * Call this early in application startup
 */
void initializePremiumFeatures();
void shutdownPremiumFeatures();