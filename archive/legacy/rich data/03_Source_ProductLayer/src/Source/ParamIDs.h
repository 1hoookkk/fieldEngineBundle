// Source/ParamIDs.h
#pragma once

//==============================================================================
/**
 * Parameter ID Constants for 7-file core
 * Centralized parameter identifier definitions
 */
namespace ParamIDs
{
    // Master section
    inline constexpr auto masterGain = "masterGain";
    inline constexpr auto paintActive = "paintActive";
    inline constexpr auto processingMode = "processingMode";

    // Heartbeat Slice section (core paint-to-audio engine)
    inline constexpr auto CPS = "CPS";
    inline constexpr auto Gamma = "Gamma";
    inline constexpr auto TopN = "TopN";

    // Paint engine section
    inline constexpr auto brushSize = "brushSize";
    inline constexpr auto pressureSensitivity = "pressureSensitivity";
    inline constexpr auto colorIntensity = "colorIntensity";
    inline constexpr auto frequencyRange = "frequencyRange";
    inline constexpr auto paintDecay = "paintDecay";
    inline constexpr auto paintMode = "paintMode";
    inline constexpr auto spatialWidth = "spatialWidth";
    inline constexpr auto quantizeToKey = "quantizeToKey";

    // Synthesis engine section
    inline constexpr auto oscillatorCount = "oscillatorCount";
    inline constexpr auto spectralMode = "spectralMode";
    inline constexpr auto topNBands = "topNBands";
    inline constexpr auto filterCutoff = "filterCutoff";
    inline constexpr auto filterResonance = "filterResonance";
    inline constexpr auto spectralMorph = "spectralMorph";
    inline constexpr auto harmonicContent = "harmonicContent";

    // Effects section
    inline constexpr auto reverbAmount = "reverbAmount";
    inline constexpr auto delayAmount = "delayAmount";
    inline constexpr auto distortionAmount = "distortionAmount";
    inline constexpr auto chorusAmount = "chorusAmount";

    // Performance section
    inline constexpr auto cpuLimit = "cpuLimit";
    inline constexpr auto qualityMode = "qualityMode";
    inline constexpr auto latencyMs = "latencyMs";
    inline constexpr auto adaptivePerformance = "adaptivePerformance";

    // Layer management section
    inline constexpr auto activeLayer = "activeLayer";
    inline constexpr auto layerOpacity = "layerOpacity";
    inline constexpr auto layerBlendMode = "layerBlendMode";

    // Mask snapshot section
    inline constexpr auto maskBlend = "maskBlend";
    inline constexpr auto maskStrength = "maskStrength";
    inline constexpr auto featherTime = "featherTime";
    inline constexpr auto featherFreq = "featherFreq";
    inline constexpr auto threshold = "threshold";
    inline constexpr auto protectHarmonics = "protectHarmonics";
}