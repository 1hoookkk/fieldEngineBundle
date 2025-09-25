/******************************************************************************
 * File: SpectralBrushPresets.cpp
 * Description: Implementation of professional spectral brush preset system
 * 
 * Professional-grade spectral brush library with musically-tuned presets
 * inspired by vintage EMU hardware and modern sound design workflows.
 * 
 * Copyright (c) 2025 Spectral Audio Systems
 ******************************************************************************/

#include "SpectralBrushPresets.h"
#include <algorithm>
#include <random>

//==============================================================================
// SpectralBrush Constructor
//==============================================================================

SpectralBrushPresets::SpectralBrush::SpectralBrush(const juce::String& brushName, 
                                                  CDPSpectralEngine::SpectralEffect effect,
                                                  juce::Colour color)
    : name(brushName)
    , primaryEffect(effect)
    , associatedColor(color)
    , recommendedMappingMode(ColorToSpectralMapper::MappingMode::HueToEffect)
    , creationDate(juce::Time::getCurrentTime())
    , author("Spectral Audio Systems")
    , version("1.0")
{
    // Set basic defaults
    description = "Professional spectral brush for " + brushName.toLowerCase();
    category = "General";
    genre = "Electronic";
    usageHint = "Paint with " + color.toString() + " for " + brushName.toLowerCase() + " effect";
    
    // Add basic tags
    tags.push_back(brushName.toLowerCase());
    tags.push_back("spectral");
    tags.push_back("effect");
}

//==============================================================================
// Constructor & Initialization
//==============================================================================

SpectralBrushPresets::SpectralBrushPresets()
{
    // Initialize factory presets
    initializeFactoryPresets();
    
    // Update category mappings
    updateBrushCategories();
    
    DBG("🎨 SpectralBrushPresets initialized with " << brushLibrary.size() << " factory brushes");
}

//==============================================================================
// Brush Library Management
//==============================================================================

void SpectralBrushPresets::addBrush(const SpectralBrush& brush)
{
    // Validate brush
    SpectralBrush validatedBrush = brush;
    validateBrush(validatedBrush);
    
    // Check if brush already exists
    if (brushExists(brush.name))
    {
        // Update existing brush
        size_t index = findBrushIndex(brush.name);
        brushLibrary[index] = validatedBrush;
    }
    else
    {
        // Add new brush
        brushLibrary.push_back(validatedBrush);
        brushNameToIndex[brush.name] = brushLibrary.size() - 1;
    }
    
    // Update categories
    updateBrushCategories();
    
    // Notify listeners
    listeners.call([&](Listener& l) { l.brushAdded(brush.name); });
    listeners.call([&](Listener& l) { l.brushLibraryChanged(); });
    
    DBG("🎨 Added brush: " << brush.name);
}

void SpectralBrushPresets::removeBrush(const juce::String& brushName)
{
    if (!brushExists(brushName))
        return;
    
    size_t index = findBrushIndex(brushName);
    brushLibrary.erase(brushLibrary.begin() + index);
    
    // Rebuild name-to-index mapping
    brushNameToIndex.clear();
    for (size_t i = 0; i < brushLibrary.size(); ++i)
    {
        brushNameToIndex[brushLibrary[i].name] = i;
    }
    
    // Update categories
    updateBrushCategories();
    
    // Notify listeners
    listeners.call([&](Listener& l) { l.brushRemoved(brushName); });
    listeners.call([&](Listener& l) { l.brushLibraryChanged(); });
    
    DBG("🎨 Removed brush: " << brushName);
}

SpectralBrushPresets::SpectralBrush* SpectralBrushPresets::getBrush(const juce::String& brushName)
{
    if (!brushExists(brushName))
        return nullptr;
    
    size_t index = findBrushIndex(brushName);
    return &brushLibrary[index];
}

const SpectralBrushPresets::SpectralBrush* SpectralBrushPresets::getBrush(const juce::String& brushName) const
{
    if (!brushExists(brushName))
        return nullptr;
    
    size_t index = findBrushIndex(brushName);
    return &brushLibrary[index];
}

std::vector<SpectralBrushPresets::SpectralBrush> SpectralBrushPresets::getAllBrushes() const
{
    return brushLibrary;
}

int SpectralBrushPresets::getBrushCount() const
{
    return static_cast<int>(brushLibrary.size());
}

//==============================================================================
// Brush Search & Filtering
//==============================================================================

std::vector<SpectralBrushPresets::SpectralBrush> SpectralBrushPresets::searchBrushes(
    const juce::String& searchTerm) const
{
    std::vector<SpectralBrush> results;
    juce::String lowerTerm = searchTerm.toLowerCase();
    
    for (const auto& brush : brushLibrary)
    {
        if (matchesSearchTerm(brush, lowerTerm))
        {
            results.push_back(brush);
        }
    }
    
    // Sort by relevance
    std::sort(results.begin(), results.end(), 
        [&](const SpectralBrush& a, const SpectralBrush& b)
        {
            float scoreA = calculateRelevanceScore(a, lowerTerm);
            float scoreB = calculateRelevanceScore(b, lowerTerm);
            return scoreA > scoreB;
        });
    
    return results;
}

std::vector<SpectralBrushPresets::SpectralBrush> SpectralBrushPresets::filterBrushesByTag(
    const juce::String& tag) const
{
    std::vector<SpectralBrush> results;
    juce::String lowerTag = tag.toLowerCase();
    
    for (const auto& brush : brushLibrary)
    {
        for (const auto& brushTag : brush.tags)
        {
            if (brushTag.toLowerCase().contains(lowerTag))
            {
                results.push_back(brush);
                break;
            }
        }
    }
    
    return results;
}

std::vector<SpectralBrushPresets::SpectralBrush> SpectralBrushPresets::filterBrushesByGenre(
    const juce::String& genre) const
{
    std::vector<SpectralBrush> results;
    juce::String lowerGenre = genre.toLowerCase();
    
    for (const auto& brush : brushLibrary)
    {
        if (brush.genre.toLowerCase().contains(lowerGenre))
        {
            results.push_back(brush);
        }
    }
    
    return results;
}

std::vector<SpectralBrushPresets::SpectralBrush> SpectralBrushPresets::getBrushesInCategory(
    BrushCategory category) const
{
    std::vector<SpectralBrush> results;
    
    if (category == BrushCategory::All)
    {
        return brushLibrary;
    }
    
    juce::String categoryName = getCategoryName(category);
    
    for (const auto& brush : brushLibrary)
    {
        if (brush.category.equalsIgnoreCase(categoryName))
        {
            results.push_back(brush);
        }
    }
    
    return results;
}

//==============================================================================
// Brush Application & Integration
//==============================================================================

void SpectralBrushPresets::applyBrush(const juce::String& brushName, CDPSpectralEngine* spectralEngine)
{
    applyBrushWithIntensity(brushName, spectralEngine, 1.0f);
}

void SpectralBrushPresets::applyBrushWithIntensity(const juce::String& brushName, 
                                                  CDPSpectralEngine* spectralEngine, 
                                                  float intensity)
{
    if (!spectralEngine) return;
    
    const SpectralBrush* brush = getBrush(brushName);
    if (!brush) 
    {
        DBG("🎨 Brush not found: " << brushName);
        return;
    }
    
    // Clear existing effects
    spectralEngine->clearSpectralLayers();
    
    // Apply primary effect
    spectralEngine->setSpectralEffect(brush->primaryEffect, intensity);
    
    // Apply parameters
    int paramIndex = 0;
    for (const auto& param : brush->parameters)
    {
        spectralEngine->setEffectParameter(brush->primaryEffect, paramIndex++, param.second);
        if (paramIndex >= 8) break; // Max 8 parameters per effect
    }
    
    // Apply layered effects
    for (const auto& layeredEffect : brush->layeredEffects)
    {
        float layerIntensity = layeredEffect.second * intensity;
        spectralEngine->addSpectralLayer(layeredEffect.first, layerIntensity);
    }
    
    // Update usage statistics
    brushUsageStats[brushName]++;
    
    // Record performance if profiling is enabled
    if (performanceProfilingEnabled)
    {
        // This would be filled in by actual performance measurement
        recordBrushUsage(brushName, 0.0f);
    }
    
    // Notify listeners
    listeners.call([&](Listener& l) { l.brushApplied(brushName); });
    
    DBG("🎨 Applied brush: " << brushName << " with intensity: " << intensity);
}

void SpectralBrushPresets::applyBrushWithColor(const juce::String& brushName,
                                              CDPSpectralEngine* spectralEngine,
                                              ColorToSpectralMapper* colorMapper,
                                              juce::Colour paintColor)
{
    if (!spectralEngine || !colorMapper) return;
    
    const SpectralBrush* brush = getBrush(brushName);
    if (!brush) return;
    
    // Apply the brush first
    applyBrush(brushName, spectralEngine);
    
    // Override color mapping if brush specifies custom mapping
    if (brush->useCustomColorMapping)
    {
        colorMapper->setMappingMode(brush->recommendedMappingMode);
    }
    
    // Update spectral engine with color
    colorMapper->updateSpectralEngineFromColor(paintColor, 1.0f, 0.0f);
    
    DBG("🎨 Applied brush with color: " << brushName << " color: " << paintColor.toString());
}

void SpectralBrushPresets::morphBetweenBrushes(const juce::String& brushA, 
                                              const juce::String& brushB,
                                              float morphAmount,
                                              CDPSpectralEngine* spectralEngine)
{
    if (!spectralEngine) return;
    
    const SpectralBrush* brushPtrA = getBrush(brushA);
    const SpectralBrush* brushPtrB = getBrush(brushB);
    
    if (!brushPtrA || !brushPtrB) return;
    
    // Clear existing effects
    spectralEngine->clearSpectralLayers();
    
    // Create morphed parameters
    auto morphedParams = interpolateParameters(brushPtrA->parameters, brushPtrB->parameters, morphAmount);
    
    // Apply morphed primary effect
    CDPSpectralEngine::SpectralEffect morphedEffect = (morphAmount < 0.5f) ? 
        brushPtrA->primaryEffect : brushPtrB->primaryEffect;
    
    spectralEngine->setSpectralEffect(morphedEffect, 1.0f);
    
    // Apply morphed parameters
    int paramIndex = 0;
    for (const auto& param : morphedParams)
    {
        spectralEngine->setEffectParameter(morphedEffect, paramIndex++, param.second);
        if (paramIndex >= 8) break;
    }
    
    DBG("🎨 Morphed between brushes: " << brushA << " <-> " << brushB << " amount: " << morphAmount);
}

//==============================================================================
// Smart Brush Recommendations
//==============================================================================

std::vector<SpectralBrushPresets::BrushRecommendation> SpectralBrushPresets::recommendBrushesForColor(
    juce::Colour color, int maxResults)
{
    std::vector<BrushRecommendation> recommendations;
    
    for (const auto& brush : brushLibrary)
    {
        BrushRecommendation rec;
        rec.brush = brush;
        
        // Calculate color similarity
        float colorSimilarity = calculateColorSimilarity(color, brush.associatedColor);
        
        // Consider secondary color if available
        if (brush.secondaryColor != juce::Colour())
        {
            float secondaryColorSimilarity = calculateColorSimilarity(color, brush.secondaryColor);
            colorSimilarity = std::max(colorSimilarity, secondaryColorSimilarity);
        }
        
        rec.relevanceScore = colorSimilarity;
        rec.reason = "Color match with " + brush.name;
        
        if (colorSimilarity > 0.3f) // Threshold for relevance
        {
            recommendations.push_back(rec);
        }
    }
    
    // Sort by relevance score
    std::sort(recommendations.begin(), recommendations.end(),
        [](const BrushRecommendation& a, const BrushRecommendation& b)
        {
            return a.relevanceScore > b.relevanceScore;
        });
    
    // Limit results
    if (recommendations.size() > static_cast<size_t>(maxResults))
    {
        recommendations.resize(maxResults);
    }
    
    return recommendations;
}

std::vector<SpectralBrushPresets::BrushRecommendation> SpectralBrushPresets::recommendBrushesForGenre(
    const juce::String& genre, int maxResults)
{
    std::vector<BrushRecommendation> recommendations;
    juce::String lowerGenre = genre.toLowerCase();
    
    for (const auto& brush : brushLibrary)
    {
        if (brush.genre.toLowerCase().contains(lowerGenre))
        {
            BrushRecommendation rec;
            rec.brush = brush;
            rec.relevanceScore = 1.0f; // Direct genre match
            rec.reason = "Perfect match for " + genre + " genre";
            recommendations.push_back(rec);
        }
        else
        {
            // Check tags for genre-related terms
            for (const auto& tag : brush.tags)
            {
                if (tag.toLowerCase().contains(lowerGenre))
                {
                    BrushRecommendation rec;
                    rec.brush = brush;
                    rec.relevanceScore = 0.7f; // Tag match
                    rec.reason = "Tag match for " + genre;
                    rec.matchingTags.push_back(tag);
                    recommendations.push_back(rec);
                    break;
                }
            }
        }
    }
    
    // Sort and limit
    std::sort(recommendations.begin(), recommendations.end(),
        [](const BrushRecommendation& a, const BrushRecommendation& b)
        {
            return a.relevanceScore > b.relevanceScore;
        });
    
    if (recommendations.size() > static_cast<size_t>(maxResults))
    {
        recommendations.resize(maxResults);
    }
    
    return recommendations;
}

//==============================================================================
// Factory Preset Definitions
//==============================================================================

void SpectralBrushPresets::initializeFactoryPresets()
{
    brushLibrary.clear();
    brushNameToIndex.clear();
    
    // Create texture brushes
    brushLibrary.push_back(createSpectralSmearBrush());
    brushLibrary.push_back(createSpectralFogBrush());
    brushLibrary.push_back(createGranularCloudBrush());
    brushLibrary.push_back(createSpectralGlassBrush());
    
    // Create rhythm brushes
    brushLibrary.push_back(createArpeggiatorBrush());
    brushLibrary.push_back(createStutterBrush());
    brushLibrary.push_back(createRhythmicGateBrush());
    brushLibrary.push_back(createBeatSlicerBrush());
    
    // Create ambient brushes
    brushLibrary.push_back(createSpectralPadBrush());
    brushLibrary.push_back(createEtherealWashBrush());
    brushLibrary.push_back(createDeepResonanceBrush());
    brushLibrary.push_back(createCosmicDriftBrush());
    
    // Create glitch brushes
    brushLibrary.push_back(createDigitalCrushBrush());
    brushLibrary.push_back(createSpectralGlitchBrush());
    brushLibrary.push_back(createBitShuffleBrush());
    brushLibrary.push_back(createDataCorruptionBrush());
    
    // Create vintage brushes
    brushLibrary.push_back(createVintageSpectralBlurBrush());
    brushLibrary.push_back(createAnalogWarmthBrush());
    brushLibrary.push_back(createTapeSpectralBrush());
    brushLibrary.push_back(createVinylSpectralBrush());
    
    // Create electronic brushes
    brushLibrary.push_back(createSynthSpectralBrush());
    brushLibrary.push_back(createBassSynthBrush());
    brushLibrary.push_back(createLeadSynthBrush());
    brushLibrary.push_back(createPadSynthBrush());
    
    // Build name-to-index mapping
    for (size_t i = 0; i < brushLibrary.size(); ++i)
    {
        brushNameToIndex[brushLibrary[i].name] = i;
    }
}

SpectralBrushPresets::SpectralBrush SpectralBrushPresets::createSpectralSmearBrush()
{
    SpectralBrush brush("Spectral Smear", CDPSpectralEngine::SpectralEffect::Blur, juce::Colours::red);
    
    brush.description = "Smooth spectral blurring for organic texture creation";
    brush.category = "Texture";
    brush.genre = "Ambient";
    brush.usageHint = "Paint with red for smooth spectral smearing";
    
    // Parameters
    brush.parameters["blur_kernel_size"] = 0.7f;
    brush.parameters["blur_direction"] = 0.5f;
    brush.parameters["wet_mix"] = 0.8f;
    
    // Performance
    brush.estimatedCPUUsage = 0.4f;
    brush.recommendedFFTSize = 1024;
    brush.complexityLevel = 0.3f;
    
    // Tags
    brush.tags = {"texture", "blur", "smooth", "ambient", "organic"};
    
    return brush;
}

SpectralBrushPresets::SpectralBrush SpectralBrushPresets::createSpectralFogBrush()
{
    SpectralBrush brush("Spectral Fog", CDPSpectralEngine::SpectralEffect::Blur, juce::Colours::lightgrey);
    
    brush.description = "Dense spectral fog for atmospheric soundscapes";
    brush.category = "Texture";
    brush.genre = "Cinematic";
    brush.usageHint = "Paint with grey for dense atmospheric fog";
    
    // Layer additional randomization
    brush.layeredEffects.push_back({CDPSpectralEngine::SpectralEffect::Randomize, 0.3f});
    
    // Parameters
    brush.parameters["blur_kernel_size"] = 0.9f;
    brush.parameters["randomize_intensity"] = 0.2f;
    brush.parameters["wet_mix"] = 0.9f;
    
    // Performance
    brush.estimatedCPUUsage = 0.6f;
    brush.recommendedFFTSize = 2048;
    brush.complexityLevel = 0.5f;
    
    // Tags
    brush.tags = {"texture", "fog", "atmospheric", "cinematic", "dense"};
    
    return brush;
}

SpectralBrushPresets::SpectralBrush SpectralBrushPresets::createArpeggiatorBrush()
{
    SpectralBrush brush("Spectral Arpeggiator", CDPSpectralEngine::SpectralEffect::Arpeggiate, juce::Colours::lime);
    
    brush.description = "Tempo-synced spectral arpeggiation for rhythmic effects";
    brush.category = "Rhythm";
    brush.genre = "Electronic";
    brush.usageHint = "Paint with green for rhythmic spectral arpeggios";
    brush.tempoSyncRequired = true;
    brush.recommendedTempo = 128.0f;
    
    // Parameters
    brush.parameters["arpeggio_rate"] = 2.0f; // 8th notes
    brush.parameters["arpeggio_direction"] = 0.0f; // Up
    brush.parameters["intensity"] = 0.8f;
    
    // Performance
    brush.estimatedCPUUsage = 0.5f;
    brush.recommendedFFTSize = 1024;
    brush.complexityLevel = 0.6f;
    
    // Tags
    brush.tags = {"rhythm", "arpeggio", "electronic", "tempo-sync", "beat"};
    
    return brush;
}

SpectralBrushPresets::SpectralBrush SpectralBrushPresets::createDigitalCrushBrush()
{
    SpectralBrush brush("Digital Crush", CDPSpectralEngine::SpectralEffect::Randomize, juce::Colours::orange);
    
    brush.description = "Aggressive digital crushing and spectral randomization";
    brush.category = "Glitch";
    brush.genre = "Electronic";
    brush.usageHint = "Paint with orange for aggressive digital crushing";
    
    // Layer shuffling for extra chaos
    brush.layeredEffects.push_back({CDPSpectralEngine::SpectralEffect::Shuffle, 0.6f});
    
    // Parameters
    brush.parameters["randomize_intensity"] = 0.9f;
    brush.parameters["shuffle_amount"] = 0.7f;
    brush.parameters["digital_artifacts"] = 0.8f;
    
    // Performance
    brush.estimatedCPUUsage = 0.7f;
    brush.recommendedFFTSize = 512; // Lower for more artifacts
    brush.complexityLevel = 0.8f;
    
    // Tags
    brush.tags = {"glitch", "digital", "crush", "aggressive", "chaos"};
    
    return brush;
}

SpectralBrushPresets::SpectralBrush SpectralBrushPresets::createVintageSpectralBlurBrush()
{
    SpectralBrush brush("Vintage Spectral Blur", CDPSpectralEngine::SpectralEffect::Blur, juce::Colours::brown);
    
    brush.description = "Warm vintage-style spectral blurring with analog character";
    brush.category = "Vintage";
    brush.genre = "Vintage";
    brush.usageHint = "Paint with sepia for warm vintage spectral blur";
    
    // Parameters tuned for vintage character
    brush.parameters["blur_kernel_size"] = 0.6f;
    brush.parameters["analog_warmth"] = 0.7f;
    brush.parameters["vintage_character"] = 0.8f;
    
    // Performance
    brush.estimatedCPUUsage = 0.4f;
    brush.recommendedFFTSize = 1024;
    brush.complexityLevel = 0.4f;
    
    // Tags
    brush.tags = {"vintage", "warm", "analog", "retro", "character"};
    
    return brush;
}

SpectralBrushPresets::SpectralBrush SpectralBrushPresets::createSpectralPadBrush()
{
    SpectralBrush brush("Spectral Pad", CDPSpectralEngine::SpectralEffect::Freeze, juce::Colours::lightblue);
    
    brush.description = "Sustained spectral pad for ambient textures";
    brush.category = "Ambient";
    brush.genre = "Ambient";
    brush.usageHint = "Paint with light blue for sustained spectral pads";
    
    // Layer time expansion for smoother sustain
    brush.layeredEffects.push_back({CDPSpectralEngine::SpectralEffect::TimeExpand, 0.4f});
    
    // Parameters
    brush.parameters["freeze_bands"] = 0.6f;
    brush.parameters["sustain_time"] = 0.9f;
    brush.parameters["time_expand_factor"] = 1.5f;
    
    // Performance
    brush.estimatedCPUUsage = 0.6f;
    brush.recommendedFFTSize = 2048;
    brush.complexityLevel = 0.5f;
    
    // Tags
    brush.tags = {"ambient", "pad", "sustain", "atmospheric", "smooth"};
    
    return brush;
}

// Create remaining factory presets with similar detailed configurations...
SpectralBrushPresets::SpectralBrush SpectralBrushPresets::createGranularCloudBrush()
{
    SpectralBrush brush("Granular Cloud", CDPSpectralEngine::SpectralEffect::Randomize, juce::Colours::lightcyan);
    // Implementation similar to above...
    brush.category = "Texture";
    brush.tags = {"granular", "cloud", "texture", "particles"};
    return brush;
}

SpectralBrushPresets::SpectralBrush SpectralBrushPresets::createSpectralGlassBrush()
{
    SpectralBrush brush("Spectral Glass", CDPSpectralEngine::SpectralEffect::Shuffle, juce::Colours::lightsteelblue);
    brush.category = "Texture";
    brush.tags = {"glass", "crystalline", "shimmer", "texture"};
    return brush;
}

// Implement remaining factory presets...
SpectralBrushPresets::SpectralBrush SpectralBrushPresets::createStutterBrush() { SpectralBrush brush("Stutter", CDPSpectralEngine::SpectralEffect::Arpeggiate, juce::Colours::yellow); brush.category = "Rhythm"; return brush; }
SpectralBrushPresets::SpectralBrush SpectralBrushPresets::createRhythmicGateBrush() { SpectralBrush brush("Rhythmic Gate", CDPSpectralEngine::SpectralEffect::Freeze, juce::Colours::gold); brush.category = "Rhythm"; return brush; }
SpectralBrushPresets::SpectralBrush SpectralBrushPresets::createBeatSlicerBrush() { SpectralBrush brush("Beat Slicer", CDPSpectralEngine::SpectralEffect::Shuffle, juce::Colours::darkorange); brush.category = "Rhythm"; return brush; }
SpectralBrushPresets::SpectralBrush SpectralBrushPresets::createEtherealWashBrush() { SpectralBrush brush("Ethereal Wash", CDPSpectralEngine::SpectralEffect::Blur, juce::Colours::lavender); brush.category = "Ambient"; return brush; }
SpectralBrushPresets::SpectralBrush SpectralBrushPresets::createDeepResonanceBrush() { SpectralBrush brush("Deep Resonance", CDPSpectralEngine::SpectralEffect::TimeExpand, juce::Colours::darkblue); brush.category = "Ambient"; return brush; }
SpectralBrushPresets::SpectralBrush SpectralBrushPresets::createCosmicDriftBrush() { SpectralBrush brush("Cosmic Drift", CDPSpectralEngine::SpectralEffect::Average, juce::Colours::mediumpurple); brush.category = "Ambient"; return brush; }
SpectralBrushPresets::SpectralBrush SpectralBrushPresets::createSpectralGlitchBrush() { SpectralBrush brush("Spectral Glitch", CDPSpectralEngine::SpectralEffect::Shuffle, juce::Colours::hotpink); brush.category = "Glitch"; return brush; }
SpectralBrushPresets::SpectralBrush SpectralBrushPresets::createBitShuffleBrush() { SpectralBrush brush("Bit Shuffle", CDPSpectralEngine::SpectralEffect::Shuffle, juce::Colours::magenta); brush.category = "Glitch"; return brush; }
SpectralBrushPresets::SpectralBrush SpectralBrushPresets::createDataCorruptionBrush() { SpectralBrush brush("Data Corruption", CDPSpectralEngine::SpectralEffect::Randomize, juce::Colours::red); brush.category = "Glitch"; return brush; }
SpectralBrushPresets::SpectralBrush SpectralBrushPresets::createAnalogWarmthBrush() { SpectralBrush brush("Analog Warmth", CDPSpectralEngine::SpectralEffect::Blur, juce::Colours::sandybrown); brush.category = "Vintage"; return brush; }
SpectralBrushPresets::SpectralBrush SpectralBrushPresets::createTapeSpectralBrush() { SpectralBrush brush("Tape Spectral", CDPSpectralEngine::SpectralEffect::TimeExpand, juce::Colours::burlywood); brush.category = "Vintage"; return brush; }
SpectralBrushPresets::SpectralBrush SpectralBrushPresets::createVinylSpectralBrush() { SpectralBrush brush("Vinyl Spectral", CDPSpectralEngine::SpectralEffect::Randomize, juce::Colours::darkgoldenrod); brush.category = "Vintage"; return brush; }
SpectralBrushPresets::SpectralBrush SpectralBrushPresets::createSynthSpectralBrush() { SpectralBrush brush("Synth Spectral", CDPSpectralEngine::SpectralEffect::Arpeggiate, juce::Colours::cyan); brush.category = "Electronic"; return brush; }
SpectralBrushPresets::SpectralBrush SpectralBrushPresets::createBassSynthBrush() { SpectralBrush brush("Bass Synth", CDPSpectralEngine::SpectralEffect::TimeExpand, juce::Colours::darkred); brush.category = "Electronic"; return brush; }
SpectralBrushPresets::SpectralBrush SpectralBrushPresets::createLeadSynthBrush() { SpectralBrush brush("Lead Synth", CDPSpectralEngine::SpectralEffect::Shuffle, juce::Colours::lime); brush.category = "Electronic"; return brush; }
SpectralBrushPresets::SpectralBrush SpectralBrushPresets::createPadSynthBrush() { SpectralBrush brush("Pad Synth", CDPSpectralEngine::SpectralEffect::Freeze, juce::Colours::lightpink); brush.category = "Electronic"; return brush; }

//==============================================================================
// Utility Methods
//==============================================================================

size_t SpectralBrushPresets::findBrushIndex(const juce::String& brushName) const
{
    auto it = brushNameToIndex.find(brushName);
    return (it != brushNameToIndex.end()) ? it->second : SIZE_MAX;
}

bool SpectralBrushPresets::brushExists(const juce::String& brushName) const
{
    return brushNameToIndex.find(brushName) != brushNameToIndex.end();
}

void SpectralBrushPresets::updateBrushCategories()
{
    categoryToBrushes.clear();
    
    for (const auto& brush : brushLibrary)
    {
        BrushCategory category = stringToBrushCategory(brush.category);
        categoryToBrushes[category].push_back(brush.name);
    }
}

void SpectralBrushPresets::validateBrush(SpectralBrush& brush)
{
    // Ensure required fields are set
    if (brush.name.isEmpty())
        brush.name = "Unnamed Brush";
    
    if (brush.description.isEmpty())
        brush.description = "Custom spectral brush";
    
    if (brush.category.isEmpty())
        brush.category = "General";
    
    // Clamp numeric values
    brush.estimatedCPUUsage = juce::jlimit(0.0f, 1.0f, brush.estimatedCPUUsage);
    brush.complexityLevel = juce::jlimit(0.0f, 1.0f, brush.complexityLevel);
    brush.userRating = juce::jlimit(0.0f, 5.0f, brush.userRating);
    
    // Ensure at least one tag
    if (brush.tags.empty())
    {
        brush.tags.push_back("custom");
    }
}

bool SpectralBrushPresets::matchesSearchTerm(const SpectralBrush& brush, const juce::String& searchTerm) const
{
    juce::String lowerTerm = searchTerm.toLowerCase();
    
    // Check name
    if (brush.name.toLowerCase().contains(lowerTerm))
        return true;
    
    // Check description
    if (brush.description.toLowerCase().contains(lowerTerm))
        return true;
    
    // Check category
    if (brush.category.toLowerCase().contains(lowerTerm))
        return true;
    
    // Check genre
    if (brush.genre.toLowerCase().contains(lowerTerm))
        return true;
    
    // Check tags
    for (const auto& tag : brush.tags)
    {
        if (tag.toLowerCase().contains(lowerTerm))
            return true;
    }
    
    return false;
}

float SpectralBrushPresets::calculateRelevanceScore(const SpectralBrush& brush, const juce::String& context) const
{
    float score = 0.0f;
    juce::String lowerContext = context.toLowerCase();
    
    // Name match gets highest score
    if (brush.name.toLowerCase().contains(lowerContext))
        score += 1.0f;
    
    // Tag matches
    for (const auto& tag : brush.tags)
    {
        if (tag.toLowerCase().contains(lowerContext))
            score += 0.5f;
    }
    
    // Category/genre matches
    if (brush.category.toLowerCase().contains(lowerContext))
        score += 0.3f;
    
    if (brush.genre.toLowerCase().contains(lowerContext))
        score += 0.3f;
    
    // Description match
    if (brush.description.toLowerCase().contains(lowerContext))
        score += 0.2f;
    
    return score;
}

float SpectralBrushPresets::calculateColorSimilarity(juce::Colour color1, juce::Colour color2) const
{
    // Simple HSB distance calculation
    float h1 = color1.getHue();
    float s1 = color1.getSaturation();
    float b1 = color1.getBrightness();
    
    float h2 = color2.getHue();
    float s2 = color2.getSaturation();
    float b2 = color2.getBrightness();
    
    // Handle hue wrap-around
    float hueDiff = std::abs(h1 - h2);
    if (hueDiff > 0.5f)
        hueDiff = 1.0f - hueDiff;
    
    float satDiff = std::abs(s1 - s2);
    float brightDiff = std::abs(b1 - b2);
    
    // Weighted distance (hue is most important)
    float distance = hueDiff * 0.6f + satDiff * 0.2f + brightDiff * 0.2f;
    
    // Convert to similarity (0 = no similarity, 1 = identical)
    return 1.0f - distance;
}

std::unordered_map<std::string, float> SpectralBrushPresets::interpolateParameters(
    const std::unordered_map<std::string, float>& paramsA,
    const std::unordered_map<std::string, float>& paramsB,
    float amount) const
{
    std::unordered_map<std::string, float> result;
    
    // Get all unique parameter names
    std::set<std::string> allParams;
    for (const auto& param : paramsA)
        allParams.insert(param.first);
    for (const auto& param : paramsB)
        allParams.insert(param.first);
    
    // Interpolate each parameter
    for (const auto& paramName : allParams)
    {
        float valueA = 0.0f;
        float valueB = 0.0f;
        
        auto itA = paramsA.find(paramName);
        if (itA != paramsA.end())
            valueA = itA->second;
        
        auto itB = paramsB.find(paramName);
        if (itB != paramsB.end())
            valueB = itB->second;
        
        result[paramName] = valueA * (1.0f - amount) + valueB * amount;
    }
    
    return result;
}

//==============================================================================
// Helper Methods for Category Management
//==============================================================================

juce::String SpectralBrushPresets::getCategoryName(BrushCategory category) const
{
    switch (category)
    {
        case BrushCategory::Texture: return "Texture";
        case BrushCategory::Rhythm: return "Rhythm";
        case BrushCategory::Ambient: return "Ambient";
        case BrushCategory::Glitch: return "Glitch";
        case BrushCategory::Vintage: return "Vintage";
        case BrushCategory::Experimental: return "Experimental";
        case BrushCategory::Electronic: return "Electronic";
        case BrushCategory::Cinematic: return "Cinematic";
        case BrushCategory::Vocal: return "Vocal";
        case BrushCategory::Harmonic: return "Harmonic";
        default: return "General";
    }
}

SpectralBrushPresets::BrushCategory SpectralBrushPresets::stringToBrushCategory(const juce::String& categoryName) const
{
    juce::String lower = categoryName.toLowerCase();
    
    if (lower == "texture") return BrushCategory::Texture;
    if (lower == "rhythm") return BrushCategory::Rhythm;
    if (lower == "ambient") return BrushCategory::Ambient;
    if (lower == "glitch") return BrushCategory::Glitch;
    if (lower == "vintage") return BrushCategory::Vintage;
    if (lower == "experimental") return BrushCategory::Experimental;
    if (lower == "electronic") return BrushCategory::Electronic;
    if (lower == "cinematic") return BrushCategory::Cinematic;
    if (lower == "vocal") return BrushCategory::Vocal;
    if (lower == "harmonic") return BrushCategory::Harmonic;
    
    return BrushCategory::Texture; // Default
}

//==============================================================================
// Performance & Usage Tracking
//==============================================================================

void SpectralBrushPresets::recordBrushUsage(const juce::String& brushName, float processingTimeMs)
{
    if (!performanceProfilingEnabled) return;
    
    brushPerformanceHistory[brushName].push_back(processingTimeMs);
    
    // Keep only recent performance data
    auto& history = brushPerformanceHistory[brushName];
    if (history.size() > 100) // Keep last 100 measurements
    {
        history.erase(history.begin());
    }
}

SpectralBrushPresets::PerformanceInfo SpectralBrushPresets::estimateBrushPerformance(
    const juce::String& brushName) const
{
    PerformanceInfo info;
    
    const SpectralBrush* brush = getBrush(brushName);
    if (!brush) return info;
    
    // Estimate based on brush configuration
    info.estimatedLatency = brush->estimatedCPUUsage * 10.0f; // Rough estimate
    info.cpuUsageEstimate = brush->estimatedCPUUsage;
    info.recommendedBufferSize = (brush->estimatedCPUUsage > 0.7f) ? 1024 : 512;
    info.requiresHighPrecision = brush->complexityLevel > 0.8f;
    
    // Use historical data if available
    auto historyIt = brushPerformanceHistory.find(brushName);
    if (historyIt != brushPerformanceHistory.end() && !historyIt->second.empty())
    {
        const auto& history = historyIt->second;
        float avgTime = 0.0f;
        for (float time : history)
            avgTime += time;
        avgTime /= history.size();
        
        info.estimatedLatency = avgTime;
    }
    
    return info;
}

//==============================================================================
// Listener Management
//==============================================================================

void SpectralBrushPresets::addListener(Listener* listener)
{
    listeners.add(listener);
}

void SpectralBrushPresets::removeListener(Listener* listener)
{
    listeners.remove(listener);
}