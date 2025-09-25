/******************************************************************************
 * File: SessionManager.cpp
 * Description: Implementation of professional session management
 * 
 * Copyright (c) 2025 Spectral Audio Systems
 ******************************************************************************/

#include "SessionManager.h"
#include <fstream>

//==============================================================================
// ProjectState Implementation
//==============================================================================

juce::ValueTree ProjectState::toValueTree() const
{
    juce::ValueTree tree("SpectralCanvasProject");
    
    // File format version
    tree.setProperty("fileVersion", 1, nullptr);
    tree.setProperty("appVersion", "1.0.0", nullptr);
    
    // Project metadata
    juce::ValueTree infoTree("ProjectInfo");
    infoTree.setProperty("name", projectInfo.projectName, nullptr);
    infoTree.setProperty("description", projectInfo.description, nullptr);
    infoTree.setProperty("author", projectInfo.author, nullptr);
    infoTree.setProperty("createdTime", projectInfo.createdTime.toMilliseconds(), nullptr);
    infoTree.setProperty("lastModifiedTime", projectInfo.lastModifiedTime.toMilliseconds(), nullptr);
    infoTree.setProperty("version", projectInfo.version, nullptr);
    infoTree.setProperty("genre", projectInfo.genre, nullptr);
    infoTree.setProperty("mood", projectInfo.mood, nullptr);
    infoTree.setProperty("key", projectInfo.key, nullptr);
    infoTree.setProperty("bpm", projectInfo.bpm, nullptr);
    infoTree.setProperty("sampleRate", projectInfo.sampleRate, nullptr);
    infoTree.setProperty("bufferSize", projectInfo.bufferSize, nullptr);
    infoTree.setProperty("numChannels", projectInfo.numChannels, nullptr);
    tree.addChild(infoTree, -1, nullptr);
    
    // Canvas and layer state
    if (layerManagerState.isValid())
        tree.addChild(layerManagerState.createCopy(), -1, nullptr);
    
    if (canvasSettings.isValid())
        tree.addChild(canvasSettings.createCopy(), -1, nullptr);
        
    if (visualSettings.isValid())
        tree.addChild(visualSettings.createCopy(), -1, nullptr);
    
    // Audio engine states
    if (spectralEngineState.isValid())
        tree.addChild(spectralEngineState.createCopy(), -1, nullptr);
        
    if (forgeProcessorState.isValid())
        tree.addChild(forgeProcessorState.createCopy(), -1, nullptr);
        
    if (effectsChainState.isValid())
        tree.addChild(effectsChainState.createCopy(), -1, nullptr);
        
    if (masterMixState.isValid())
        tree.addChild(masterMixState.createCopy(), -1, nullptr);
    
    // Active presets
    if (!activePresets.empty())
    {
        juce::ValueTree presetsTree("ActivePresets");
        for (const auto& preset : activePresets)
        {
            juce::ValueTree presetTree("Preset");
            presetTree.setProperty("id", preset.presetId, nullptr);
            presetTree.setProperty("name", preset.presetName, nullptr);
            presetTree.setProperty("category", preset.category, nullptr);
            presetTree.setProperty("isUserPreset", preset.isUserPreset, nullptr);
            if (preset.parameters.isValid())
                presetTree.addChild(preset.parameters.createCopy(), -1, nullptr);
            presetsTree.addChild(presetTree, -1, nullptr);
        }
        tree.addChild(presetsTree, -1, nullptr);
    }
    
    if (userPresets.isValid())
        tree.addChild(userPresets.createCopy(), -1, nullptr);
    
    // Sample references
    if (!sampleReferences.empty())
    {
        juce::ValueTree samplesTree("SampleReferences");
        for (const auto& sample : sampleReferences)
        {
            juce::ValueTree sampleTree("Sample");
            sampleTree.setProperty("originalPath", sample.originalPath, nullptr);
            sampleTree.setProperty("embeddedId", sample.embeddedId, nullptr);
            sampleTree.setProperty("sampleName", sample.sampleName, nullptr);
            sampleTree.setProperty("fileSize", static_cast<int>(sample.fileSize), nullptr);
            sampleTree.setProperty("hash", sample.hash, nullptr);
            sampleTree.setProperty("isEmbedded", sample.isEmbedded, nullptr);
            samplesTree.addChild(sampleTree, -1, nullptr);
        }
        tree.addChild(samplesTree, -1, nullptr);
    }
    
    // Automation data
    if (!automationData.empty())
    {
        juce::ValueTree automationTree("Automation");
        for (const auto& lane : automationData)
        {
            juce::ValueTree laneTree("AutomationLane");
            laneTree.setProperty("parameterId", lane.parameterId, nullptr);
            laneTree.setProperty("displayName", lane.displayName, nullptr);
            laneTree.setProperty("isEnabled", lane.isEnabled, nullptr);
            
            juce::ValueTree keyframesTree("Keyframes");
            for (const auto& keyframe : lane.keyframes)
            {
                juce::ValueTree kfTree("Keyframe");
                kfTree.setProperty("time", keyframe.first, nullptr);
                kfTree.setProperty("value", keyframe.second, nullptr);
                keyframesTree.addChild(kfTree, -1, nullptr);
            }
            laneTree.addChild(keyframesTree, -1, nullptr);
            automationTree.addChild(laneTree, -1, nullptr);
        }
        tree.addChild(automationTree, -1, nullptr);
    }
    
    return tree;
}

void ProjectState::fromValueTree(const juce::ValueTree& tree)
{
    if (!tree.hasType("SpectralCanvasProject"))
        return;
    
    // Verify file version
    int fileVersion = tree.getProperty("fileVersion", 0);
    if (fileVersion > 1)
    {
        DBG("WARNING: Project file version " << fileVersion << " is newer than supported version 1");
    }
    
    // Load project info
    auto infoTree = tree.getChildWithName("ProjectInfo");
    if (infoTree.isValid())
    {
        projectInfo.projectName = infoTree.getProperty("name", "Untitled");
        projectInfo.description = infoTree.getProperty("description", "");
        projectInfo.author = infoTree.getProperty("author", "");
        projectInfo.createdTime = juce::Time(infoTree.getProperty("createdTime", 0));
        projectInfo.lastModifiedTime = juce::Time(infoTree.getProperty("lastModifiedTime", 0));
        projectInfo.version = infoTree.getProperty("version", "1.0");
        projectInfo.genre = infoTree.getProperty("genre", "");
        projectInfo.mood = infoTree.getProperty("mood", "");
        projectInfo.key = infoTree.getProperty("key", "C Major");
        projectInfo.bpm = infoTree.getProperty("bpm", 120.0f);
        projectInfo.sampleRate = infoTree.getProperty("sampleRate", 44100.0);
        projectInfo.bufferSize = infoTree.getProperty("bufferSize", 512);
        projectInfo.numChannels = infoTree.getProperty("numChannels", 2);
    }
    
    // Load canvas states
    layerManagerState = tree.getChildWithName("LayerManager");
    canvasSettings = tree.getChildWithName("CanvasSettings");
    visualSettings = tree.getChildWithName("VisualSettings");
    
    // Load audio engine states
    spectralEngineState = tree.getChildWithName("SpectralEngineState");
    forgeProcessorState = tree.getChildWithName("ForgeProcessorState");
    effectsChainState = tree.getChildWithName("EffectsChainState");
    masterMixState = tree.getChildWithName("MasterMixState");
    
    // Load active presets
    auto presetsTree = tree.getChildWithName("ActivePresets");
    if (presetsTree.isValid())
    {
        activePresets.clear();
        for (int i = 0; i < presetsTree.getNumChildren(); ++i)
        {
            auto presetTree = presetsTree.getChild(i);
            PresetReference preset;
            preset.presetId = presetTree.getProperty("id", "");
            preset.presetName = presetTree.getProperty("name", "");
            preset.category = presetTree.getProperty("category", "");
            preset.isUserPreset = presetTree.getProperty("isUserPreset", false);
            preset.parameters = presetTree.getChildWithName("Parameters");
            activePresets.push_back(preset);
        }
    }
    
    userPresets = tree.getChildWithName("UserPresets");
    
    // Load sample references
    auto samplesTree = tree.getChildWithName("SampleReferences");
    if (samplesTree.isValid())
    {
        sampleReferences.clear();
        for (int i = 0; i < samplesTree.getNumChildren(); ++i)
        {
            auto sampleTree = samplesTree.getChild(i);
            SampleReference sample;
            sample.originalPath = sampleTree.getProperty("originalPath", "");
            sample.embeddedId = sampleTree.getProperty("embeddedId", "");
            sample.sampleName = sampleTree.getProperty("sampleName", "");
            sample.fileSize = static_cast<int64_t>(static_cast<int>(sampleTree.getProperty("fileSize", 0)));
            sample.hash = sampleTree.getProperty("hash", "");
            sample.isEmbedded = sampleTree.getProperty("isEmbedded", false);
            sampleReferences.push_back(sample);
        }
    }
    
    // Load automation data
    auto automationTree = tree.getChildWithName("Automation");
    if (automationTree.isValid())
    {
        automationData.clear();
        for (int i = 0; i < automationTree.getNumChildren(); ++i)
        {
            auto laneTree = automationTree.getChild(i);
            AutomationLane lane;
            lane.parameterId = laneTree.getProperty("parameterId", "");
            lane.displayName = laneTree.getProperty("displayName", "");
            lane.isEnabled = laneTree.getProperty("isEnabled", true);
            
            auto keyframesTree = laneTree.getChildWithName("Keyframes");
            if (keyframesTree.isValid())
            {
                for (int j = 0; j < keyframesTree.getNumChildren(); ++j)
                {
                    auto kfTree = keyframesTree.getChild(j);
                    double time = kfTree.getProperty("time", 0.0);
                    float value = kfTree.getProperty("value", 0.0f);
                    lane.keyframes.emplace_back(time, value);
                }
            }
            automationData.push_back(lane);
        }
    }
}

bool ProjectState::isValid() const
{
    return !projectInfo.projectName.isEmpty() && 
           layerManagerState.isValid();
}

int64_t ProjectState::calculateStorageSize() const
{
    // Rough estimate of serialized size
    int64_t size = 0;
    
    // Base project info
    size += projectInfo.projectName.length() * 2;
    size += projectInfo.description.length() * 2;
    
    // Layer data (approximate)
    size += 1024;  // Base layer structure
    
    // Sample references
    for (const auto& sample : sampleReferences)
    {
        size += sample.originalPath.length() * 2;
        size += sample.sampleName.length() * 2;
        if (sample.isEmbedded)
            size += sample.fileSize;
    }
    
    // Embedded samples
    size += embeddedSamples.getSize();
    
    // Automation data
    size += automationData.size() * 100;  // Rough estimate per lane
    
    return size;
}

juce::String ProjectState::calculateFileHash(const juce::File& file)
{
    if (!file.existsAsFile())
        return {};
    
    juce::FileInputStream stream(file);
    if (!stream.openedOk())
        return {};
    
    // Use MD5 for file hashing
    juce::MD5 md5(stream);
    return md5.toHexString();
}

//==============================================================================
// SessionManager Implementation
//==============================================================================

SessionManager::SessionManager()
{
    // Initialize folders
    auto appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    auto spectralCanvasFolder = appData.getChildFile("SpectralCanvas Pro");
    
    preferences.defaultProjectsFolder = spectralCanvasFolder.getChildFile("Projects");
    preferences.templatesFolder = spectralCanvasFolder.getChildFile("Templates");
    preferences.autoSaveFolder = spectralCanvasFolder.getChildFile("AutoSave");
    preferences.crashRecoveryFolder = spectralCanvasFolder.getChildFile("CrashRecovery");
    
    // Create directories if they don't exist
    preferences.defaultProjectsFolder.createDirectory();
    preferences.templatesFolder.createDirectory();
    preferences.autoSaveFolder.createDirectory();
    preferences.crashRecoveryFolder.createDirectory();
    
    // Load preferences
    loadPreferences();
    loadRecentProjects();
    
    // Start auto-save timer if enabled
    if (autoSaveEnabled.load())
    {
        startTimer(autoSaveIntervalMinutes.load() * 60 * 1000);
    }
}

SessionManager::~SessionManager()
{
    stopTimer();
    savePreferences();
    saveRecentProjects();
}

SessionManager::SaveResult SessionManager::saveProject(const juce::File& file, 
                                                       const ProjectState& state, 
                                                       bool embedSamples)
{
    if (!file.hasWriteAccess())
        return SaveResult::PermissionError;
    
    // Create backup if file exists
    if (file.existsAsFile() && preferences.backupOnSave)
    {
        createBackup(file);
    }
    
    // Write project file
    auto result = writeProjectFile(file, state, embedSamples);
    
    if (result == SaveResult::Success)
    {
        currentProjectFile = file;
        projectModified.store(false);
        addToRecentProjects(file);
        
        if (onProjectSaved)
            onProjectSaved(file.getFileNameWithoutExtension());
    }
    
    return result;
}

SessionManager::LoadResult SessionManager::loadProject(const juce::File& file, ProjectState& outState)
{
    if (!file.existsAsFile())
        return LoadResult::FileNotFound;
    
    if (!validateProjectFile(file))
        return LoadResult::FormatError;
    
    auto result = readProjectFile(file, outState);
    
    if (result == LoadResult::Success)
    {
        currentProjectFile = file;
        projectModified.store(false);
        addToRecentProjects(file);
        
        if (onProjectLoaded)
            onProjectLoaded(file.getFileNameWithoutExtension());
    }
    
    return result;
}

SessionManager::SaveResult SessionManager::quickSave()
{
    if (!hasActiveProject())
        return SaveResult::FileError;
    
    // This would need to get current state from the application
    // For now, return success as placeholder
    return SaveResult::Success;
}

void SessionManager::newProject()
{
    if (hasActiveProject() && projectModified.load())
    {
        if (onSavePrompt && onSavePrompt("Save changes to current project?"))
        {
            quickSave();
        }
    }
    
    currentProjectFile = juce::File();
    projectModified.store(false);
    
    if (onProjectClosed)
        onProjectClosed("New Project");
}

bool SessionManager::closeProject(bool promptToSave)
{
    if (hasActiveProject() && projectModified.load() && promptToSave)
    {
        if (onSavePrompt && onSavePrompt("Save changes before closing?"))
        {
            auto result = quickSave();
            if (result != SaveResult::Success)
                return false;  // User might want to cancel
        }
    }
    
    auto projectName = getCurrentProjectName();
    currentProjectFile = juce::File();
    projectModified.store(false);
    
    if (onProjectClosed)
        onProjectClosed(projectName);
    
    return true;
}

void SessionManager::enableAutoSave(bool enable, int intervalMinutes)
{
    autoSaveEnabled.store(enable);
    autoSaveIntervalMinutes.store(intervalMinutes);
    
    if (enable)
    {
        startTimer(intervalMinutes * 60 * 1000);
    }
    else
    {
        stopTimer();
    }
}

void SessionManager::timerCallback()
{
    if (autoSaveEnabled.load() && hasActiveProject() && projectModified.load())
    {
        performAutoSave();
    }
}

juce::File SessionManager::getAutoSaveFile() const
{
    if (!hasActiveProject())
        return {};
    
    auto fileName = getCurrentProjectName() + "_autosave" + PROJECT_FILE_EXTENSION;
    return preferences.autoSaveFolder.getChildFile(fileName);
}

void SessionManager::performAutoSave()
{
    if (autoSaveInProgress.load() || !stateProvider)
        return;
    
    autoSaveInProgress.store(true);
    
    // Perform auto-save on background thread to avoid audio thread interference
    backgroundOperations.addJob([this]() {
        try
        {
            if (hasActiveProject())
            {
                // Capture current state
                ProjectState autoSaveState;
                autoSaveState.layerManagerState = stateProvider->captureLayerManagerState();
                autoSaveState.spectralEngineState = stateProvider->captureSpectralEngineState();
                autoSaveState.canvasSettings = stateProvider->captureCanvasSettings();
                autoSaveState.masterMixState = stateProvider->captureAudioSettings();
                
                autoSaveState.projectInfo.projectName = getCurrentProjectName() + "_autosave";
                autoSaveState.updateLastModified();
                
                // Save to auto-save location
                auto autoSaveFile = getAutoSaveFile();
                auto result = writeProjectFile(autoSaveFile, autoSaveState, false);  // Don't embed samples in auto-save
                
                if (result == SaveResult::Success)
                {
                    lastAutoSave = juce::Time::getCurrentTime();
                    
                    if (onAutoSaveCompleted)
                        onAutoSaveCompleted("Auto-save completed: " + autoSaveFile.getFileName());
                }
                else
                {
                    DBG("Auto-save failed with result: " << static_cast<int>(result));
                }
            }
        }
        catch (const std::exception& e)
        {
            DBG("ERROR during auto-save: " << e.what());
        }
        
        autoSaveInProgress.store(false);
    });
}

// Implementation for reloading current session
SessionManager::LoadResult SessionManager::reloadCurrentSession()
{
    if (!hasActiveProject())
        return LoadResult::FileNotFound;
    
    if (!stateProvider)
    {
        DBG("ERROR: No ApplicationStateProvider set - cannot restore state");
        return LoadResult::FormatError;
    }
    
    ProjectState reloadedState;
    auto result = loadProject(currentProjectFile, reloadedState);
    
    if (result == LoadResult::Success)
    {
        try
        {
            stateProvider->restoreApplicationState(reloadedState);
        }
        catch (const std::exception& e)
        {
            DBG("ERROR during state restoration: " << e.what());
            return LoadResult::CorruptedData;
        }
    }
    
    return result;
}

void SessionManager::addToRecentProjects(const juce::File& file)
{
    juce::ScopedLock sl(recentProjectsLock);
    
    // Remove if already exists
    recentProjects.erase(
        std::remove(recentProjects.begin(), recentProjects.end(), file),
        recentProjects.end());
    
    // Add to front
    recentProjects.insert(recentProjects.begin(), file);
    
    // Limit size
    if (recentProjects.size() > preferences.maxRecentProjects)
    {
        recentProjects.resize(preferences.maxRecentProjects);
    }
}

std::vector<juce::File> SessionManager::getRecentProjects() const
{
    juce::ScopedLock sl(recentProjectsLock);
    
    // Filter out non-existent files
    std::vector<juce::File> validProjects;
    for (const auto& file : recentProjects)
    {
        if (file.existsAsFile())
            validProjects.push_back(file);
    }
    
    return validProjects;
}

juce::String SessionManager::getCurrentProjectName() const
{
    if (!hasActiveProject())
        return "Untitled";
    
    return currentProjectFile.getFileNameWithoutExtension();
}

SessionManager::SaveResult SessionManager::writeProjectFile(const juce::File& file,
                                                            const ProjectState& state,
                                                            bool embedSamples)
{
    try
    {
        juce::FileOutputStream output(file);
        if (!output.openedOk())
            return SaveResult::FileError;
        
        // Write header
        output.write(PROJECT_FILE_HEADER, strlen(PROJECT_FILE_HEADER));
        output.writeInt(CURRENT_PROJECT_VERSION);
        
        // Serialize project state
        auto tree = state.toValueTree();
        juce::MemoryOutputStream treeData;
        tree.writeToStream(treeData);
        
        // Write compressed data
        juce::GZIPCompressorOutputStream compressedOutput(&output);
        compressedOutput.write(treeData.getData(), treeData.getDataSize());
        compressedOutput.flush();
        
        return SaveResult::Success;
    }
    catch (...)
    {
        return SaveResult::SerializationError;
    }
}

SessionManager::LoadResult SessionManager::readProjectFile(const juce::File& file, ProjectState& outState)
{
    try
    {
        juce::FileInputStream input(file);
        if (!input.openedOk())
            return LoadResult::FileNotFound;
        
        // Read and verify header
        char header[32];
        input.read(header, strlen(PROJECT_FILE_HEADER));
        if (strncmp(header, PROJECT_FILE_HEADER, strlen(PROJECT_FILE_HEADER)) != 0)
            return LoadResult::FormatError;
        
        int version = input.readInt();
        if (!checkProjectVersion(version))
            return LoadResult::VersionMismatch;
        
        // Read compressed data
        juce::GZIPDecompressorInputStream decompressedInput(&input, false);
        juce::ValueTree tree = juce::ValueTree::readFromStream(decompressedInput);
        
        if (!tree.isValid())
            return LoadResult::CorruptedData;
        
        // Restore project state
        outState.fromValueTree(tree);
        
        if (!outState.isValid())
            return LoadResult::CorruptedData;
        
        return LoadResult::Success;
    }
    catch (...)
    {
        return LoadResult::CorruptedData;
    }
}

void SessionManager::createBackup(const juce::File& originalFile)
{
    auto backupFile = originalFile.getSiblingFile(originalFile.getFileNameWithoutExtension() + 
                                                  "_backup" + 
                                                  PROJECT_FILE_EXTENSION);
    originalFile.copyFileTo(backupFile);
    
    // Clean up old backups
    cleanupOldBackups(originalFile);
}

void SessionManager::cleanupOldBackups(const juce::File& projectFile)
{
    auto parentDir = projectFile.getParentDirectory();
    auto baseName = projectFile.getFileNameWithoutExtension();
    
    juce::Array<juce::File> backupFiles;
    parentDir.findChildFiles(backupFiles, 
                             juce::File::findFiles, 
                             false, 
                             baseName + "_backup*" + PROJECT_FILE_EXTENSION,
                             juce::File::FollowSymlinks::yes);
    
    // Keep only the 5 most recent backups
    if (backupFiles.size() > 5)
    {
        // Sort by modification time (newest first) - JUCE requires comparator class
        struct FileTimeComparator
        {
            int compareElements(const juce::File& a, const juce::File& b) const
            {
                if (a.getLastModificationTime() > b.getLastModificationTime())
                    return -1;
                else if (a.getLastModificationTime() < b.getLastModificationTime())
                    return 1;
                else
                    return 0;
            }
        };
        
        FileTimeComparator comparator;
        backupFiles.sort(comparator);
        
        // Delete older backups
        for (int i = 5; i < backupFiles.size(); ++i)
        {
            backupFiles[i].deleteFile();
        }
    }
}

bool SessionManager::validateProjectFile(const juce::File& file) const
{
    if (!file.existsAsFile() || file.getSize() < 32)
        return false;
    
    juce::FileInputStream input(file);
    if (!input.openedOk())
        return false;
    
    char header[32];
    input.read(header, strlen(PROJECT_FILE_HEADER));
    
    return strncmp(header, PROJECT_FILE_HEADER, strlen(PROJECT_FILE_HEADER)) == 0;
}

bool SessionManager::checkProjectVersion(int version) const
{
    return version >= 1 && version <= CURRENT_PROJECT_VERSION;
}

void SessionManager::loadPreferences()
{
    auto appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    auto prefsFile = appData.getChildFile("SpectralCanvas Pro").getChildFile("preferences.xml");
    
    if (prefsFile.existsAsFile())
    {
        auto xml = juce::parseXML(prefsFile);
        if (xml != nullptr)
        {
            auto root = xml->getChildByName("SpectralCanvasPreferences");
            if (root != nullptr)
            {
                preferences.autoSaveEnabled = root->getBoolAttribute("autoSaveEnabled", true);
                preferences.autoSaveIntervalMinutes = root->getIntAttribute("autoSaveInterval", 5);
                preferences.crashRecoveryEnabled = root->getBoolAttribute("crashRecoveryEnabled", true);
                preferences.embedSamplesByDefault = root->getBoolAttribute("embedSamplesByDefault", false);
                preferences.backupOnSave = root->getBoolAttribute("backupOnSave", true);
                preferences.maxRecentProjects = root->getIntAttribute("maxRecentProjects", 10);
                preferences.maxUndoLevels = root->getIntAttribute("maxUndoLevels", 30);
            }
        }
    }
}

void SessionManager::savePreferences()
{
    auto appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    auto prefsFile = appData.getChildFile("SpectralCanvas Pro").getChildFile("preferences.xml");
    
    auto xml = std::make_unique<juce::XmlElement>("SpectralCanvasPreferences");
    xml->setAttribute("autoSaveEnabled", preferences.autoSaveEnabled);
    xml->setAttribute("autoSaveInterval", preferences.autoSaveIntervalMinutes);
    xml->setAttribute("crashRecoveryEnabled", preferences.crashRecoveryEnabled);
    xml->setAttribute("embedSamplesByDefault", preferences.embedSamplesByDefault);
    xml->setAttribute("backupOnSave", preferences.backupOnSave);
    xml->setAttribute("maxRecentProjects", preferences.maxRecentProjects);
    xml->setAttribute("maxUndoLevels", preferences.maxUndoLevels);
    
    xml->writeTo(prefsFile);
}

void SessionManager::loadRecentProjects()
{
    auto appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    auto recentFile = appData.getChildFile("SpectralCanvas Pro").getChildFile("recent_projects.xml");
    
    if (recentFile.existsAsFile())
    {
        auto xml = juce::parseXML(recentFile);
        if (xml != nullptr)
        {
            juce::ScopedLock sl(recentProjectsLock);
            recentProjects.clear();
            
            for (auto* child : xml->getChildIterator())
            {
                if (child->hasTagName("Project"))
                {
                    auto path = child->getStringAttribute("path");
                    if (path.isNotEmpty())
                    {
                        juce::File file(path);
                        if (file.existsAsFile())
                            recentProjects.push_back(file);
                    }
                }
            }
        }
    }
}

void SessionManager::saveRecentProjects()
{
    auto appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    auto recentFile = appData.getChildFile("SpectralCanvas Pro").getChildFile("recent_projects.xml");
    
    juce::ScopedLock sl(recentProjectsLock);
    
    auto xml = std::make_unique<juce::XmlElement>("RecentProjects");
    for (const auto& file : recentProjects)
    {
        if (file.existsAsFile())
        {
            auto projectElement = xml->createNewChildElement("Project");
            projectElement->setAttribute("path", file.getFullPathName());
        }
    }
    
    xml->writeTo(recentFile);
}