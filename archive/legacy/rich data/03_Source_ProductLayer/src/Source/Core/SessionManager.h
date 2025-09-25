/******************************************************************************
 * File: SessionManager.h
 * Description: Professional session management for SpectralCanvas Pro
 * 
 * Provides complete project state management, auto-save, crash recovery,
 * and professional workflow features to transform SpectralCanvas Pro into
 * a production-ready MetaSynth competitor.
 * 
 * Copyright (c) 2025 Spectral Audio Systems
 ******************************************************************************/

#pragma once
#include <JuceHeader.h>
#include <memory>
#include <atomic>
#include <chrono>
#include "CanvasLayer.h"
#include "SpectralBrushPresets.h"

//==============================================================================
/**
 * @brief Complete project state container
 * 
 * Holds all data needed to perfectly restore a SpectralCanvas Pro session,
 * including canvas layers, preset configurations, effect states, and metadata.
 */
class ProjectState
{
public:
    //==============================================================================
    // Project Metadata
    
    struct ProjectInfo
    {
        juce::String projectName = "Untitled";
        juce::String description;
        juce::String author;
        juce::Time createdTime;
        juce::Time lastModifiedTime;
        juce::String version = "1.0";
        
        // Creative metadata
        juce::String genre;
        juce::String mood;
        juce::String key = "C Major";
        float bpm = 120.0f;
        
        // Technical metadata
        double sampleRate = 44100.0;
        int bufferSize = 512;
        int numChannels = 2;
        
        ProjectInfo()
        {
            auto now = juce::Time::getCurrentTime();
            createdTime = now;
            lastModifiedTime = now;
        }
    };
    
    ProjectInfo projectInfo;
    
    //==============================================================================
    // Canvas State
    
    juce::ValueTree layerManagerState;      // Complete layer hierarchy
    juce::ValueTree canvasSettings;        // Canvas view settings (zoom, pan, etc.)
    juce::ValueTree visualSettings;        // Theme, colors, display preferences
    
    //==============================================================================
    // Audio Engine State
    
    juce::ValueTree spectralEngineState;   // SpectralSynthEngine configuration
    juce::ValueTree forgeProcessorState;   // ForgeProcessor sample slots
    juce::ValueTree effectsChainState;     // All effects and their parameters
    juce::ValueTree masterMixState;        // Master output settings
    
    //==============================================================================
    // Preset System State
    
    struct PresetReference
    {
        juce::String presetId;
        juce::String presetName;
        juce::String category;
        bool isUserPreset = false;
        juce::ValueTree parameters;        // Current parameter overrides
    };
    
    std::vector<PresetReference> activePresets;
    juce::ValueTree userPresets;           // User-created presets
    
    //==============================================================================
    // Sample References
    
    struct SampleReference
    {
        juce::String originalPath;         // Original file path
        juce::String embeddedId;          // ID if embedded in project
        juce::String sampleName;
        int64_t fileSize = 0;
        juce::String hash;                // For integrity checking
        bool isEmbedded = false;          // Whether sample data is in project
    };
    
    std::vector<SampleReference> sampleReferences;
    juce::MemoryBlock embeddedSamples;    // Embedded sample data
    
    //==============================================================================
    // Automation Data
    
    struct AutomationLane
    {
        juce::String parameterId;
        juce::String displayName;
        std::vector<std::pair<double, float>> keyframes;  // time, value pairs
        bool isEnabled = true;
    };
    
    std::vector<AutomationLane> automationData;
    
    //==============================================================================
    // Methods
    
    ProjectState() = default;
    
    // Serialization
    juce::ValueTree toValueTree() const;
    void fromValueTree(const juce::ValueTree& tree);
    
    // State management
    void updateLastModified() { projectInfo.lastModifiedTime = juce::Time::getCurrentTime(); }
    bool isValid() const;
    int64_t calculateStorageSize() const;
    
    // Sample management
    void addSampleReference(const juce::File& file, bool embed = false);
    juce::File resolveSamplePath(const SampleReference& ref, const juce::File& projectFile) const;
    bool validateSampleIntegrity() const;
    
private:
    static juce::String calculateFileHash(const juce::File& file);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProjectState)
};

//==============================================================================
/**
 * @brief Professional session management system
 * 
 * Handles project file operations, auto-save, crash recovery, templates,
 * and all session-related workflows for professional production use.
 */
class SessionManager : public juce::Timer
{
public:
    SessionManager();
    ~SessionManager() override;
    
    //==============================================================================
    // Project File Operations
    
    enum class SaveResult
    {
        Success,
        FileError,
        PermissionError,
        SampleError,
        SerializationError,
        UserCancelled
    };
    
    enum class LoadResult  
    {
        Success,
        FileNotFound,
        FormatError,
        VersionMismatch,
        SamplesMissing,
        CorruptedData,
        UserCancelled
    };
    
    // Core operations
    SaveResult saveProject(const juce::File& file, const ProjectState& state, bool embedSamples = false);
    LoadResult loadProject(const juce::File& file, ProjectState& outState);
    
    // Quick save/load for current session
    SaveResult quickSave();
    LoadResult reloadCurrentSession();
    
    // New/Close
    void newProject();
    bool closeProject(bool promptToSave = true);
    
    //==============================================================================
    // Auto-Save System
    
    void enableAutoSave(bool enable, int intervalMinutes = 5);
    bool isAutoSaveEnabled() const { return autoSaveEnabled.load(); }
    int getAutoSaveInterval() const { return autoSaveIntervalMinutes.load(); }
    
    void timerCallback() override;  // For auto-save
    
    juce::File getAutoSaveFile() const;
    bool hasAutoSaveData() const;
    LoadResult recoverFromAutoSave(ProjectState& outState);
    void clearAutoSaveData();
    
    //==============================================================================
    // Recent Projects
    
    void addToRecentProjects(const juce::File& file);
    std::vector<juce::File> getRecentProjects() const;
    void clearRecentProjects();
    
    //==============================================================================
    // Project Templates
    
    struct ProjectTemplate
    {
        juce::String name;
        juce::String description;
        juce::String category;
        juce::File templateFile;
        juce::Image thumbnail;
        
        // Template metadata
        juce::String genre;
        float bpm = 120.0f;
        juce::String key = "C Major";
    };
    
    std::vector<ProjectTemplate> getAvailableTemplates() const;
    SaveResult saveAsTemplate(const ProjectState& state, const juce::String& name, 
                             const juce::String& description, const juce::String& category);
    LoadResult createFromTemplate(const juce::String& templateName, ProjectState& outState);
    
    //==============================================================================
    // Current Session State
    
    bool hasActiveProject() const { return currentProjectFile.existsAsFile(); }
    juce::File getCurrentProjectFile() const { return currentProjectFile; }
    juce::String getCurrentProjectName() const;
    bool isProjectModified() const { return projectModified.load(); }
    
    void markProjectModified(bool modified = true) { projectModified.store(modified); }
    
    //==============================================================================
    // Import/Export
    
    // Import from other formats
    LoadResult importMetaSynthProject(const juce::File& file, ProjectState& outState);
    LoadResult importCDPProject(const juce::File& file, ProjectState& outState);
    LoadResult importAudioFiles(const std::vector<juce::File>& files, ProjectState& outState);
    
    // Export to other formats
    SaveResult exportCanvasAsImage(const juce::File& file, int width = 1920, int height = 1080);
    SaveResult exportProjectAsStemWAV(const juce::File& outputFolder);
    SaveResult exportProjectSettings(const juce::File& file, const ProjectState& state);
    
    //==============================================================================
    // Project Statistics & Analysis
    
    struct ProjectStatistics
    {
        // Canvas statistics
        int totalLayers = 0;
        int totalStrokes = 0;
        int totalSamples = 0;
        float averagePressure = 0.0f;
        
        // Audio statistics
        int activeOscillators = 0;
        int loadedSamples = 0;
        int64_t totalSampleSize = 0;
        
        // Performance statistics
        float averageCPUUsage = 0.0f;
        int64_t memoryUsage = 0;
        
        // Creative statistics
        std::map<juce::String, int> colorUsage;
        std::map<juce::String, int> effectUsage;
        juce::Time totalEditTime;
    };
    
    ProjectStatistics analyzeProject(const ProjectState& state) const;
    
    //==============================================================================
    // Crash Recovery
    
    void enableCrashRecovery(bool enable);
    bool hasCrashRecoveryData() const;
    LoadResult recoverFromCrash(ProjectState& outState);
    void clearCrashRecoveryData();
    
    //==============================================================================
    // Settings & Preferences
    
    struct SessionPreferences
    {
        bool autoSaveEnabled = true;
        int autoSaveIntervalMinutes = 5;
        bool crashRecoveryEnabled = true;
        bool embedSamplesByDefault = false;
        bool backupOnSave = true;
        int maxRecentProjects = 10;
        int maxUndoLevels = 30;
        
        // Paths
        juce::File defaultProjectsFolder;
        juce::File templatesFolder;
        juce::File autoSaveFolder;
        juce::File crashRecoveryFolder;
    };
    
    SessionPreferences& getPreferences() { return preferences; }
    void savePreferences();
    void loadPreferences();
    
    //==============================================================================
    //==============================================================================
    // Core System Integration
    
    // Application state capture interface
    struct ApplicationStateProvider
    {
        virtual ~ApplicationStateProvider() = default;
        virtual juce::ValueTree captureLayerManagerState() = 0;
        virtual juce::ValueTree captureSpectralEngineState() = 0;
        virtual juce::ValueTree captureCanvasSettings() = 0;
        virtual juce::ValueTree captureAudioSettings() = 0;
        virtual void restoreApplicationState(const ProjectState& state) = 0;
    };
    
    void setApplicationStateProvider(ApplicationStateProvider* provider) { stateProvider = provider; }
    
    //==============================================================================
    // Event Callbacks
    
    std::function<void(const juce::String&)> onProjectLoaded;
    std::function<void(const juce::String&)> onProjectSaved;
    std::function<void()> onProjectModified;
    std::function<void(const juce::String&)> onProjectClosed;
    std::function<void(const juce::String&)> onAutoSaveCompleted;
    std::function<bool(const juce::String&)> onSavePrompt;  // Return true to save, false to discard
    
private:
    //==============================================================================
    // Internal State
    
    juce::File currentProjectFile;
    std::atomic<bool> projectModified{false};
    std::atomic<bool> autoSaveEnabled{true};
    std::atomic<int> autoSaveIntervalMinutes{5};
    
    // Recent projects (thread-safe)
    mutable juce::CriticalSection recentProjectsLock;
    std::vector<juce::File> recentProjects;
    
    // Preferences
    SessionPreferences preferences;
    
    // Auto-save tracking
    juce::Time lastAutoSave;
    std::atomic<bool> autoSaveInProgress{false};
    
    // Background operations for audio thread safety
    juce::ThreadPool backgroundOperations{2};
    
    // Application state integration
    ApplicationStateProvider* stateProvider = nullptr;
    
    //==============================================================================
    // File Format Constants
    
    static constexpr char PROJECT_FILE_HEADER[] = "SPECTRALCANVAS_PRO_PROJECT";
    static constexpr int CURRENT_PROJECT_VERSION = 1;
    static constexpr char PROJECT_FILE_EXTENSION[] = ".scp";
    static constexpr char TEMPLATE_FILE_EXTENSION[] = ".scpt";
    
    //==============================================================================
    // Internal Methods
    
    // File operations
    SaveResult writeProjectFile(const juce::File& file, const ProjectState& state, bool embedSamples);
    LoadResult readProjectFile(const juce::File& file, ProjectState& outState);
    
    // Backup management
    void createBackup(const juce::File& originalFile);
    void cleanupOldBackups(const juce::File& projectFile);
    
    // Sample management
    SaveResult embedSamplesInProject(const ProjectState& state, juce::MemoryOutputStream& output);
    LoadResult extractEmbeddedSamples(const juce::MemoryInputStream& input, ProjectState& state);
    
    // Auto-save implementation
    void performAutoSave();
    juce::File getAutoSaveFolder() const;
    
    // Recent projects management
    void updateRecentProjects(const juce::File& file);
    void saveRecentProjects();
    void loadRecentProjects();
    
    // Crash recovery implementation  
    void saveCrashRecoveryData(const ProjectState& state);
    juce::File getCrashRecoveryFolder() const;
    
    // Validation
    bool validateProjectFile(const juce::File& file) const;
    bool checkProjectVersion(int version) const;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SessionManager)
};