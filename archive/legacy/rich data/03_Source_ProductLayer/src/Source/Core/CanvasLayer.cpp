/******************************************************************************
 * File: CanvasLayer.cpp
 * Description: Implementation of multi-layer canvas system
 * 
 * Copyright (c) 2025 Spectral Audio Systems
 ******************************************************************************/

#include "CanvasLayer.h"

//==============================================================================
// CanvasLayer Implementation
//==============================================================================

CanvasLayer::CanvasLayer(int layerIndex, const juce::String& layerName)
    : index(layerIndex),
      name(layerName.isEmpty() ? "Layer " + juce::String(layerIndex + 1) : layerName)
{
}

//==============================================================================
// Paint Stroke Management

void CanvasLayer::addPaintStroke(const PaintStroke& stroke)
{
    if (locked.load()) return;
    
    juce::ScopedLock sl(strokeLock);
    paintStrokes.push_back(stroke);
    invalidateCache();
}

void CanvasLayer::clearStrokes()
{
    if (locked.load()) return;
    
    juce::ScopedLock sl(strokeLock);
    paintStrokes.clear();
    currentStroke.reset();
    invalidateCache();
}

void CanvasLayer::beginStroke(juce::Point<float> position, juce::Colour color, float pressure)
{
    if (locked.load()) return;
    
    juce::ScopedLock sl(strokeLock);
    currentStroke = std::make_unique<PaintStroke>(color, pressure);
    currentStroke->path.startNewSubPath(position);
    currentStroke->pressures.push_back(pressure);
}

void CanvasLayer::continueStroke(juce::Point<float> position, float pressure)
{
    if (locked.load() || !currentStroke) return;
    
    juce::ScopedLock sl(strokeLock);
    currentStroke->path.lineTo(position);
    currentStroke->pressures.push_back(pressure);
    invalidateCache();
}

void CanvasLayer::endStroke()
{
    if (locked.load() || !currentStroke) return;
    
    juce::ScopedLock sl(strokeLock);
    paintStrokes.push_back(*currentStroke);
    currentStroke.reset();
    invalidateCache();
}

void CanvasLayer::removeLastStroke()
{
    if (locked.load()) return;
    
    juce::ScopedLock sl(strokeLock);
    if (!paintStrokes.empty())
    {
        paintStrokes.pop_back();
        invalidateCache();
    }
}

CanvasLayer::PaintStroke* CanvasLayer::getCurrentStroke()
{
    return currentStroke.get();
}

//==============================================================================
// Rendering

void CanvasLayer::render(juce::Graphics& g, juce::Rectangle<float> bounds) const
{
    if (!visible.load() || opacity.load() <= 0.01f) return;
    
    // Update cache if needed
    if (!cacheValid)
    {
        updateCache(bounds);
    }
    
    // Apply blend mode and opacity
    float layerOpacity = opacity.load();
    BlendMode mode = blendMode.load();
    
    if (mode == BlendMode::Normal)
    {
        // Simple alpha blending for normal mode
        g.setOpacity(layerOpacity);
        g.drawImageAt(cachedImage, bounds.getX(), bounds.getY());
    }
    else
    {
        // Complex blend modes require pixel manipulation
        renderWithBlendMode(g, bounds, cachedImage);
    }
}

void CanvasLayer::renderWithBlendMode(juce::Graphics& g, juce::Rectangle<float> bounds, 
                                      juce::Image& targetImage) const
{
    // This would implement complex blend modes
    // For now, fallback to normal blending
    g.setOpacity(opacity.load());
    g.drawImageAt(targetImage, bounds.getX(), bounds.getY());
}

void CanvasLayer::updateCache(juce::Rectangle<float> bounds) const
{
    juce::ScopedLock sl(strokeLock);
    
    // Create or resize cache image
    int width = static_cast<int>(bounds.getWidth());
    int height = static_cast<int>(bounds.getHeight());
    
    if (cachedImage.getWidth() != width || cachedImage.getHeight() != height)
    {
        cachedImage = juce::Image(juce::Image::ARGB, width, height, true);
    }
    else
    {
        cachedImage.clear(cachedImage.getBounds());
    }
    
    // Render strokes to cache
    juce::Graphics cacheG(cachedImage);
    
    for (const auto& stroke : paintStrokes)
    {
        // Apply pressure-based width variation
        float avgPressure = 0.0f;
        if (!stroke.pressures.empty())
        {
            for (float p : stroke.pressures)
                avgPressure += p;
            avgPressure /= stroke.pressures.size();
        }
        else
        {
            avgPressure = stroke.intensity;
        }
        
        // Draw stroke with glow effect
        float strokeWidth = 1.0f + (avgPressure * 3.0f);
        
        // Outer glow
        cacheG.setColour(stroke.color.withAlpha(0.2f));
        cacheG.strokePath(stroke.path, juce::PathStrokeType(strokeWidth * 2.0f));
        
        // Main stroke
        cacheG.setColour(stroke.color.withAlpha(0.8f * stroke.intensity));
        cacheG.strokePath(stroke.path, juce::PathStrokeType(strokeWidth));
        
        // Inner highlight
        cacheG.setColour(stroke.color.brighter(0.5f).withAlpha(0.6f * stroke.intensity));
        cacheG.strokePath(stroke.path, juce::PathStrokeType(strokeWidth * 0.5f));
    }
    
    // Render current stroke if any
    if (currentStroke)
    {
        float currentPressure = currentStroke->pressures.empty() ? 1.0f : currentStroke->pressures.back();
        float strokeWidth = 1.0f + (currentPressure * 3.0f);
        
        cacheG.setColour(currentStroke->color.withAlpha(0.8f));
        cacheG.strokePath(currentStroke->path, juce::PathStrokeType(strokeWidth));
    }
    
    cacheValid = true;
}

//==============================================================================
// Statistics

CanvasLayer::LayerStatistics CanvasLayer::calculateStatistics() const
{
    juce::ScopedLock sl(strokeLock);
    LayerStatistics stats;
    
    stats.strokeCount = static_cast<int>(paintStrokes.size());
    
    if (stats.strokeCount > 0)
    {
        // Calculate average pressure
        float totalPressure = 0.0f;
        int pressureCount = 0;
        
        // Calculate dominant color and bounding box
        float minX = std::numeric_limits<float>::max();
        float minY = std::numeric_limits<float>::max();
        float maxX = std::numeric_limits<float>::min();
        float maxY = std::numeric_limits<float>::min();
        
        std::map<juce::uint32, int> colorFrequency;
        
        for (const auto& stroke : paintStrokes)
        {
            // Pressure statistics
            for (float p : stroke.pressures)
            {
                totalPressure += p;
                pressureCount++;
            }
            
            // Color frequency
            colorFrequency[stroke.color.getARGB()]++;
            
            // Bounding box
            auto strokeBounds = stroke.path.getBounds();
            minX = std::min(minX, strokeBounds.getX());
            minY = std::min(minY, strokeBounds.getY());
            maxX = std::max(maxX, strokeBounds.getRight());
            maxY = std::max(maxY, strokeBounds.getBottom());
        }
        
        // Calculate average pressure
        if (pressureCount > 0)
            stats.averagePressure = totalPressure / pressureCount;
        
        // Find dominant color
        int maxFreq = 0;
        juce::uint32 dominantARGB = 0;
        for (const auto& [color, freq] : colorFrequency)
        {
            if (freq > maxFreq)
            {
                maxFreq = freq;
                dominantARGB = color;
            }
        }
        stats.dominantColor = juce::Colour(dominantARGB);
        
        // Set bounding box
        stats.boundingBox = juce::Rectangle<float>(minX, minY, maxX - minX, maxY - minY);
        
        // Last modified time
        if (!paintStrokes.empty())
            stats.lastModified = paintStrokes.back().timestamp;
    }
    
    return stats;
}

//==============================================================================
// Serialization

juce::ValueTree CanvasLayer::toValueTree() const
{
    juce::ValueTree tree("Layer");
    
    tree.setProperty("index", index, nullptr);
    tree.setProperty("name", name, nullptr);
    tree.setProperty("visible", visible.load(), nullptr);
    tree.setProperty("opacity", opacity.load(), nullptr);
    tree.setProperty("blendMode", static_cast<int>(blendMode.load()), nullptr);
    tree.setProperty("locked", locked.load(), nullptr);
    tree.setProperty("solo", solo.load(), nullptr);
    tree.setProperty("muted", muted.load(), nullptr);
    
    // Audio routing
    juce::ValueTree audioTree("AudioRouting");
    audioTree.setProperty("outputChannel", audioRouting.outputChannel, nullptr);
    audioTree.setProperty("gain", audioRouting.gain, nullptr);
    audioTree.setProperty("pan", audioRouting.pan, nullptr);
    audioTree.setProperty("processEffects", audioRouting.processEffects, nullptr);
    audioTree.setProperty("effectSlot", audioRouting.effectSlot, nullptr);
    tree.addChild(audioTree, -1, nullptr);
    
    // Paint strokes
    juce::ValueTree strokesTree("Strokes");
    for (const auto& stroke : paintStrokes)
    {
        juce::ValueTree strokeTree("Stroke");
        strokeTree.setProperty("color", stroke.color.toString(), nullptr);
        strokeTree.setProperty("intensity", stroke.intensity, nullptr);
        
        // Store path as string
        juce::MemoryOutputStream pathStream;
        stroke.path.writePathToStream(pathStream);
        strokeTree.setProperty("pathData", pathStream.toUTF8(), nullptr);
        
        // Store pressures
        juce::String pressureString;
        for (float p : stroke.pressures)
            pressureString += juce::String(p) + ",";
        strokeTree.setProperty("pressures", pressureString, nullptr);
        
        strokesTree.addChild(strokeTree, -1, nullptr);
    }
    tree.addChild(strokesTree, -1, nullptr);
    
    return tree;
}

void CanvasLayer::fromValueTree(const juce::ValueTree& tree)
{
    index = tree.getProperty("index", 0);
    name = tree.getProperty("name", "Layer");
    visible.store(tree.getProperty("visible", true));
    opacity.store(tree.getProperty("opacity", 1.0f));
    blendMode.store(static_cast<BlendMode>(int(tree.getProperty("blendMode", 0))));
    locked.store(tree.getProperty("locked", false));
    solo.store(tree.getProperty("solo", false));
    muted.store(tree.getProperty("muted", false));
    
    // Audio routing
    auto audioTree = tree.getChildWithName("AudioRouting");
    if (audioTree.isValid())
    {
        audioRouting.outputChannel = audioTree.getProperty("outputChannel", 0);
        audioRouting.gain = audioTree.getProperty("gain", 1.0f);
        audioRouting.pan = audioTree.getProperty("pan", 0.0f);
        audioRouting.processEffects = audioTree.getProperty("processEffects", true);
        audioRouting.effectSlot = audioTree.getProperty("effectSlot", -1);
    }
    
    // Paint strokes
    paintStrokes.clear();
    auto strokesTree = tree.getChildWithName("Strokes");
    if (strokesTree.isValid())
    {
        for (int i = 0; i < strokesTree.getNumChildren(); ++i)
        {
            auto strokeTree = strokesTree.getChild(i);
            
            juce::Colour color = juce::Colour::fromString(strokeTree.getProperty("color").toString());
            float intensity = strokeTree.getProperty("intensity", 1.0f);
            
            PaintStroke stroke(color, intensity);
            
            // Restore path
            juce::String pathData = strokeTree.getProperty("pathData").toString();
            juce::MemoryInputStream pathStream(pathData.toRawUTF8(), pathData.getNumBytesAsUTF8(), false);
            stroke.path.loadPathFromStream(pathStream);
            
            // Restore pressures
            juce::String pressureString = strokeTree.getProperty("pressures").toString();
            juce::StringArray pressureTokens;
            pressureTokens.addTokens(pressureString, ",", "");
            for (const auto& token : pressureTokens)
            {
                if (token.isNotEmpty())
                    stroke.pressures.push_back(token.getFloatValue());
            }
            
            paintStrokes.push_back(stroke);
        }
    }
    
    invalidateCache();
}

//==============================================================================
// LayerManager Implementation
//==============================================================================

LayerManager::LayerManager()
{
    // Create initial layer
    addLayer("Background");
}

CanvasLayer* LayerManager::addLayer(const juce::String& name)
{
    juce::ScopedLock sl(layerLock);
    
    if (layers.size() >= MAX_LAYERS)
        return nullptr;
    
    auto newLayer = std::make_unique<CanvasLayer>(nextLayerIndex++, name);
    auto* layerPtr = newLayer.get();
    layers.push_back(std::move(newLayer));
    
    if (!activeLayer)
        activeLayer = layerPtr;
    
    return layerPtr;
}

void LayerManager::removeLayer(int index)
{
    juce::ScopedLock sl(layerLock);
    
    if (index < 0 || index >= layers.size())
        return;
    
    // Don't remove the last layer
    if (layers.size() <= 1)
        return;
    
    auto* removedLayer = layers[index].get();
    layers.erase(layers.begin() + index);
    
    // Update active layer if needed
    if (activeLayer == removedLayer)
    {
        activeLayer = layers.empty() ? nullptr : layers.front().get();
    }
}

void LayerManager::moveLayer(int fromIndex, int toIndex)
{
    juce::ScopedLock sl(layerLock);
    
    if (fromIndex < 0 || fromIndex >= layers.size() ||
        toIndex < 0 || toIndex >= layers.size() ||
        fromIndex == toIndex)
        return;
    
    auto layer = std::move(layers[fromIndex]);
    layers.erase(layers.begin() + fromIndex);
    layers.insert(layers.begin() + toIndex, std::move(layer));
}

void LayerManager::clearAllLayers()
{
    juce::ScopedLock sl(layerLock);
    
    for (auto& layer : layers)
        layer->clearStrokes();
}

CanvasLayer* LayerManager::getLayer(int index)
{
    juce::ScopedLock sl(layerLock);
    
    if (index < 0 || index >= layers.size())
        return nullptr;
    
    return layers[index].get();
}

const CanvasLayer* LayerManager::getLayer(int index) const
{
    juce::ScopedLock sl(layerLock);
    
    if (index < 0 || index >= layers.size())
        return nullptr;
    
    return layers[index].get();
}

void LayerManager::setActiveLayer(int index)
{
    juce::ScopedLock sl(layerLock);
    
    if (index >= 0 && index < layers.size())
        activeLayer = layers[index].get();
}

void LayerManager::renderAllLayers(juce::Graphics& g, juce::Rectangle<float> bounds) const
{
    juce::ScopedLock sl(layerLock);
    
    // Render layers from bottom to top
    for (const auto& layer : layers)
    {
        if (layer->isVisible())
        {
            // Check solo states
            if (hasAnySolo() && !layer->isSolo())
                continue;
            
            layer->render(g, bounds);
        }
    }
}

void LayerManager::updateSoloStates()
{
    // This would update audio routing based on solo states
    // Implementation depends on audio engine integration
}

bool LayerManager::hasAnySolo() const
{
    for (const auto& layer : layers)
    {
        if (layer->isSolo())
            return true;
    }
    return false;
}

juce::ValueTree LayerManager::toValueTree() const
{
    juce::ValueTree tree("LayerManager");
    
    tree.setProperty("nextLayerIndex", nextLayerIndex, nullptr);
    
    for (const auto& layer : layers)
    {
        tree.addChild(layer->toValueTree(), -1, nullptr);
    }
    
    return tree;
}

void LayerManager::fromValueTree(const juce::ValueTree& tree)
{
    juce::ScopedLock sl(layerLock);
    
    layers.clear();
    activeLayer = nullptr;
    
    nextLayerIndex = tree.getProperty("nextLayerIndex", 0);
    
    for (int i = 0; i < tree.getNumChildren(); ++i)
    {
        auto layerTree = tree.getChild(i);
        int index = layerTree.getProperty("index", i);
        juce::String name = layerTree.getProperty("name", "Layer");
        
        auto newLayer = std::make_unique<CanvasLayer>(index, name);
        newLayer->fromValueTree(layerTree);
        
        if (i == 0)
            activeLayer = newLayer.get();
        
        layers.push_back(std::move(newLayer));
    }
}