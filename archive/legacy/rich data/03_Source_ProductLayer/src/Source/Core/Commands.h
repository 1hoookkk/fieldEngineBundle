#pragma once
// ──────────────────────────────────────────────────────────────────────────────
// JUCE defines a legacy   #define CommandID int
// Undefine it before we declare our own enum:
#ifdef CommandID
#undef CommandID
#endif
// ──────────────────────────────────────────────────────────────────────────────
#include <JuceHeader.h>
#include <cstring> // For strncpy

// Unique, project-scoped identifiers -----------------------------------------
enum class ForgeCommandID
{
    // Test
    Test = 0,

    // Forge commands
    LoadSample = 10,
    StartPlayback,
    StopPlayback,
    SetPitch,
    SetSpeed,
    SetSyncMode,
    SetVolume,
    SetDrive,
    SetCrush,

    // Canvas commands (legacy - being replaced by PaintCommandID)
    LoadCanvasImage = 50,
    SetCanvasPlayhead,
    SetCanvasActive,
    SetProcessingMode,
    SetCanvasFreqRange
};

// Sample Masking Engine commands
enum class SampleMaskingCommandID
{
    LoadSample = 100,
    ClearSample,
    StartPlayback,
    StopPlayback,
    PausePlayback,
    SetLooping,
    SetPlaybackSpeed,
    SetPlaybackPosition,
    CreatePaintMask,
    AddPointToMask,
    FinalizeMask,
    RemoveMask,
    ClearAllMasks,
    SetMaskMode,
    SetMaskIntensity,
    SetMaskParameters,
    BeginPaintStroke,
    UpdatePaintStroke,
    EndPaintStroke,
    SetCanvasSize,
    SetTimeRange
};

// New Paint Engine commands
enum class PaintCommandID
{
    BeginStroke = 200,
    UpdateStroke,
    EndStroke,
    ClearCanvas,
    ClearRegion,
    SetPlayheadPosition,
    SetCanvasRegion,
    SetPaintActive,
    SetMasterGain,
    SetFrequencyRange
};

// Audio Recording commands
enum class RecordingCommandID
{
    StartRecording = 300,
    StopRecording,
    ExportToFile,
    SetRecordingFormat,
    SetRecordingDirectory
};

// FIFO message object ---------------------------------------------------------
struct Command
{
    // Command type - can be either ForgeCommandID or PaintCommandID
    int commandId = static_cast<int>(ForgeCommandID::Test);
    
    // Basic parameters
    int            intParam = -1;        // slot / index / mode
    float          floatParam = 0.0f;    // numeric value
    double         doubleParam = 0.0;    // double precision value
    bool           boolParam = false;    // flag / toggle
    char           stringParam[256];     // path / text (fixed-size to avoid dynamic allocation)
    
    // Helper method to safely set string parameter
    void setStringParam(const juce::String& str)
    {
        const char* cStr = str.toUTF8();
        std::strncpy(stringParam, cStr, sizeof(stringParam) - 1);
        stringParam[sizeof(stringParam) - 1] = '\0';
    }
    
    void setStringParam(const char* cStr)
    {
        if (cStr != nullptr)
        {
            std::strncpy(stringParam, cStr, sizeof(stringParam) - 1);
            stringParam[sizeof(stringParam) - 1] = '\0';
        }
        else
        {
            stringParam[0] = '\0'; // Empty string
        }
    }
    
    // Helper method to get string parameter as juce::String
    juce::String getStringParam() const
    {
        return juce::String(stringParam);
    }
    
    // Extended parameters for PaintEngine
    float          x = 0.0f;             // Canvas X position
    float          y = 0.0f;             // Canvas Y position  
    float          pressure = 1.0f;      // Brush pressure
    juce::Colour   color;                // Brush color

    // Constructors for Forge commands
    Command()
    {
        stringParam[0] = '\0'; // Initialize empty string
    }
    explicit Command(ForgeCommandID c) : commandId(static_cast<int>(c))
    {
        stringParam[0] = '\0';
    }
    Command(ForgeCommandID c, int s) : commandId(static_cast<int>(c)), intParam(s)
    {
        stringParam[0] = '\0';
    }
    Command(ForgeCommandID c, int s, float v) : commandId(static_cast<int>(c)), intParam(s), floatParam(v)
    {
        stringParam[0] = '\0';
    }
    Command(ForgeCommandID c, int s, bool  b) : commandId(static_cast<int>(c)), intParam(s), boolParam(b)
    {
        stringParam[0] = '\0';
    }
    Command(ForgeCommandID c, int s, const juce::String& p) : commandId(static_cast<int>(c)), intParam(s)
    {
        setStringParam(p);
    }
    Command(ForgeCommandID c, float v) : commandId(static_cast<int>(c)), floatParam(v)
    {
        stringParam[0] = '\0';
    }
    Command(ForgeCommandID c, bool b) : commandId(static_cast<int>(c)), boolParam(b)
    {
        stringParam[0] = '\0';
    }
    Command(ForgeCommandID c, const juce::String& p) : commandId(static_cast<int>(c))
    {
        setStringParam(p);
    }
    
    // Constructors for SampleMasking commands
    explicit Command(SampleMaskingCommandID c) : commandId(static_cast<int>(c))
    {
        stringParam[0] = '\0';
    }
    Command(SampleMaskingCommandID c, const juce::String& path) : commandId(static_cast<int>(c))
    {
        setStringParam(path);
    }
    Command(SampleMaskingCommandID c, int id) : commandId(static_cast<int>(c)), intParam(id)
    {
        stringParam[0] = '\0';
    }
    Command(SampleMaskingCommandID c, float value) : commandId(static_cast<int>(c)), floatParam(value)
    {
        stringParam[0] = '\0';
    }
    Command(SampleMaskingCommandID c, bool value) : commandId(static_cast<int>(c)), boolParam(value)
    {
        stringParam[0] = '\0';
    }
    Command(SampleMaskingCommandID c, int id, float x_, float y_, float pressure_ = 1.0f)
        : commandId(static_cast<int>(c)), intParam(id), x(x_), y(y_), pressure(pressure_)
    {
        stringParam[0] = '\0';
    }
    Command(SampleMaskingCommandID c, int id, int mode) : commandId(static_cast<int>(c)), intParam(id), floatParam(static_cast<float>(mode))
    {
        stringParam[0] = '\0';
    }
    Command(SampleMaskingCommandID c, float width, float height)
        : commandId(static_cast<int>(c)), floatParam(width), doubleParam(height)
    {
        stringParam[0] = '\0';
    }
    Command(SampleMaskingCommandID c, float x_, float y_, float pressure_, juce::Colour color_)
        : commandId(static_cast<int>(c)), x(x_), y(y_), pressure(pressure_), color(color_)
    {
        stringParam[0] = '\0';
    }

    // Constructors for Paint commands
    explicit Command(PaintCommandID c) : commandId(static_cast<int>(c))
    {
        stringParam[0] = '\0';
    }
    Command(PaintCommandID c, float x_, float y_, float pressure_ = 1.0f, juce::Colour color_ = juce::Colours::white)
        : commandId(static_cast<int>(c)), x(x_), y(y_), pressure(pressure_), color(color_)
    {
        stringParam[0] = '\0';
    }
    Command(PaintCommandID c, float x_, float y_, float width, float height)
        : commandId(static_cast<int>(c)), x(x_), y(y_), floatParam(width), doubleParam(height)
    {
        stringParam[0] = '\0';
    }
    Command(PaintCommandID c, float value) : commandId(static_cast<int>(c)), floatParam(value)
    {
        stringParam[0] = '\0';
    }
    Command(PaintCommandID c, bool value) : commandId(static_cast<int>(c)), boolParam(value)
    {
        stringParam[0] = '\0';
    }
    Command(PaintCommandID c, float min, float max) : commandId(static_cast<int>(c)), floatParam(min), doubleParam(max)
    {
        stringParam[0] = '\0';
    }
    
    // Constructors for Recording commands
    explicit Command(RecordingCommandID c) : commandId(static_cast<int>(c))
    {
        stringParam[0] = '\0';
    }
    Command(RecordingCommandID c, const juce::String& path) : commandId(static_cast<int>(c))
    {
        setStringParam(path);
    }
    Command(RecordingCommandID c, int format) : commandId(static_cast<int>(c)), intParam(format)
    {
        stringParam[0] = '\0';
    }
    
    // Helper methods to check command type
    bool isForgeCommand() const { return commandId < 100; }
    bool isSampleMaskingCommand() const { return commandId >= 100 && commandId < 200; }
    bool isPaintCommand() const { return commandId >= 200 && commandId < 300; }
    bool isRecordingCommand() const { return commandId >= 300; }
    ForgeCommandID getForgeCommandID() const { return static_cast<ForgeCommandID>(commandId); }
    SampleMaskingCommandID getSampleMaskingCommandID() const { return static_cast<SampleMaskingCommandID>(commandId); }
    PaintCommandID getPaintCommandID() const { return static_cast<PaintCommandID>(commandId); }
    RecordingCommandID getRecordingCommandID() const { return static_cast<RecordingCommandID>(commandId); }
};