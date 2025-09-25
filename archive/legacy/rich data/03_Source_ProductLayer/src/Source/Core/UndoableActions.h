/******************************************************************************
 * File: UndoableActions.h
 * Description: Undoable actions for SpectralCanvas Pro using JUCE UndoManager
 * 
 * Provides professional undo/redo capability for all canvas operations,
 * ensuring non-destructive workflow and creative confidence.
 * 
 * Copyright (c) 2025 Spectral Audio Systems
 ******************************************************************************/

#pragma once
#include <JuceHeader.h>
#include "CanvasLayer.h"

//==============================================================================
/**
 * @brief Base class for all undoable canvas actions
 */
class CanvasUndoableAction : public juce::UndoableAction
{
public:
    CanvasUndoableAction(LayerManager* manager) : layerManager(manager) {}
    
protected:
    LayerManager* layerManager;
};

//==============================================================================
/**
 * @brief Undoable action for adding a paint stroke to a layer
 */
class AddStrokeAction : public CanvasUndoableAction
{
public:
    AddStrokeAction(LayerManager* manager, 
                    int layerIndex,
                    const CanvasLayer::PaintStroke& stroke)
        : CanvasUndoableAction(manager),
          targetLayerIndex(layerIndex),
          paintStroke(stroke)
    {
    }
    
    bool perform() override
    {
        if (auto* layer = layerManager->getLayer(targetLayerIndex))
        {
            layer->addPaintStroke(paintStroke);
            return true;
        }
        return false;
    }
    
    bool undo() override
    {
        if (auto* layer = layerManager->getLayer(targetLayerIndex))
        {
            layer->removeLastStroke();
            return true;
        }
        return false;
    }
    
    int getSizeInUnits() override
    {
        // Estimate memory usage: path complexity + pressure data
        return static_cast<int>(sizeof(CanvasLayer::PaintStroke) + 
                               paintStroke.pressures.size() * sizeof(float));
    }
    
    juce::String getName() const { return "Add Paint Stroke"; }
    
private:
    int targetLayerIndex;
    CanvasLayer::PaintStroke paintStroke;
};

//==============================================================================
/**
 * @brief Undoable action for clearing a layer
 */
class ClearLayerAction : public CanvasUndoableAction
{
public:
    ClearLayerAction(LayerManager* manager, int layerIndex)
        : CanvasUndoableAction(manager),
          targetLayerIndex(layerIndex)
    {
        // Store the current state before clearing
        if (auto* layer = layerManager->getLayer(targetLayerIndex))
        {
            savedStrokes = layer->getStrokes();
        }
    }
    
    bool perform() override
    {
        if (auto* layer = layerManager->getLayer(targetLayerIndex))
        {
            layer->clearStrokes();
            return true;
        }
        return false;
    }
    
    bool undo() override
    {
        if (auto* layer = layerManager->getLayer(targetLayerIndex))
        {
            layer->clearStrokes();
            for (const auto& stroke : savedStrokes)
            {
                layer->addPaintStroke(stroke);
            }
            return true;
        }
        return false;
    }
    
    int getSizeInUnits() override
    {
        size_t totalSize = 0;
        for (const auto& stroke : savedStrokes)
        {
            totalSize += sizeof(CanvasLayer::PaintStroke) + 
                        stroke.pressures.size() * sizeof(float);
        }
        return static_cast<int>(totalSize);
    }
    
    juce::String getName() const { return "Clear Layer"; }
    
private:
    int targetLayerIndex;
    std::vector<CanvasLayer::PaintStroke> savedStrokes;
};

//==============================================================================
/**
 * @brief Undoable action for adding a new layer
 */
class AddLayerAction : public CanvasUndoableAction
{
public:
    AddLayerAction(LayerManager* manager, const juce::String& layerName)
        : CanvasUndoableAction(manager),
          name(layerName)
    {
    }
    
    bool perform() override
    {
        addedLayer = layerManager->addLayer(name);
        if (addedLayer)
        {
            addedLayerIndex = addedLayer->getIndex();
            return true;
        }
        return false;
    }
    
    bool undo() override
    {
        if (addedLayerIndex >= 0)
        {
            layerManager->removeLayer(addedLayerIndex);
            return true;
        }
        return false;
    }
    
    int getSizeInUnits() override { return sizeof(CanvasLayer); }
    
    juce::String getName() const { return "Add Layer"; }
    
private:
    juce::String name;
    CanvasLayer* addedLayer = nullptr;
    int addedLayerIndex = -1;
};

//==============================================================================
/**
 * @brief Undoable action for removing a layer
 */
class RemoveLayerAction : public CanvasUndoableAction
{
public:
    RemoveLayerAction(LayerManager* manager, int layerIndex)
        : CanvasUndoableAction(manager),
          targetLayerIndex(layerIndex)
    {
        // Save the layer state before removal
        if (auto* layer = layerManager->getLayer(targetLayerIndex))
        {
            savedLayerState = layer->toValueTree();
            layerName = layer->getName();
        }
    }
    
    bool perform() override
    {
        layerManager->removeLayer(targetLayerIndex);
        return true;
    }
    
    bool undo() override
    {
        // Recreate the layer from saved state
        auto* restoredLayer = layerManager->addLayer(layerName);
        if (restoredLayer && savedLayerState.isValid())
        {
            restoredLayer->fromValueTree(savedLayerState);
            return true;
        }
        return false;
    }
    
    int getSizeInUnits() override 
    { 
        return static_cast<int>(savedLayerState.getNumChildren() * 100);
    }
    
    juce::String getName() const { return "Remove Layer"; }
    
private:
    int targetLayerIndex;
    juce::String layerName;
    juce::ValueTree savedLayerState;
};

//==============================================================================
/**
 * @brief Undoable action for changing layer properties
 */
class ChangeLayerPropertyAction : public CanvasUndoableAction
{
public:
    enum PropertyType
    {
        Opacity,
        BlendMode,
        Visibility,
        Lock,
        Solo,
        Mute
    };
    
    ChangeLayerPropertyAction(LayerManager* manager,
                              int layerIndex,
                              PropertyType property,
                              const juce::var& newValue)
        : CanvasUndoableAction(manager),
          targetLayerIndex(layerIndex),
          propertyType(property),
          newPropertyValue(newValue)
    {
        // Store old value
        if (auto* layer = layerManager->getLayer(targetLayerIndex))
        {
            switch (propertyType)
            {
                case Opacity:
                    oldPropertyValue = layer->getOpacity();
                    break;
                case BlendMode:
                    oldPropertyValue = static_cast<int>(layer->getBlendMode());
                    break;
                case Visibility:
                    oldPropertyValue = layer->isVisible();
                    break;
                case Lock:
                    oldPropertyValue = layer->isLocked();
                    break;
                case Solo:
                    oldPropertyValue = layer->isSolo();
                    break;
                case Mute:
                    oldPropertyValue = layer->isMuted();
                    break;
            }
        }
    }
    
    bool perform() override
    {
        return applyValue(newPropertyValue);
    }
    
    bool undo() override
    {
        return applyValue(oldPropertyValue);
    }
    
    int getSizeInUnits() override { return sizeof(juce::var) * 2; }
    
    juce::String getName() const 
    { 
        switch (propertyType)
        {
            case Opacity: return "Change Layer Opacity";
            case BlendMode: return "Change Blend Mode";
            case Visibility: return "Toggle Layer Visibility";
            case Lock: return "Toggle Layer Lock";
            case Solo: return "Toggle Layer Solo";
            case Mute: return "Toggle Layer Mute";
            default: return "Change Layer Property";
        }
    }
    
private:
    int targetLayerIndex;
    PropertyType propertyType;
    juce::var oldPropertyValue;
    juce::var newPropertyValue;
    
    bool applyValue(const juce::var& value)
    {
        if (auto* layer = layerManager->getLayer(targetLayerIndex))
        {
            switch (propertyType)
            {
                case Opacity:
                    layer->setOpacity(static_cast<float>(value));
                    return true;
                case BlendMode:
                    layer->setBlendMode(static_cast<CanvasLayer::BlendMode>(int(value)));
                    return true;
                case Visibility:
                    layer->setVisible(static_cast<bool>(value));
                    return true;
                case Lock:
                    layer->setLocked(static_cast<bool>(value));
                    return true;
                case Solo:
                    layer->setSolo(static_cast<bool>(value));
                    return true;
                case Mute:
                    layer->setMuted(static_cast<bool>(value));
                    return true;
            }
        }
        return false;
    }
};

//==============================================================================
/**
 * @brief Manages undo/redo for the canvas with proper history limits
 */
class CanvasUndoManager
{
public:
    CanvasUndoManager(LayerManager* manager)
        : layerManager(manager)
    {
        // Set reasonable limits for undo history
        undoManager.setMaxNumberOfStoredUnits(10000, 30);  // 10MB or 30 actions
    }
    
    // Perform actions
    void addStroke(int layerIndex, const CanvasLayer::PaintStroke& stroke)
    {
        undoManager.perform(new AddStrokeAction(layerManager, layerIndex, stroke));
    }
    
    void clearLayer(int layerIndex)
    {
        undoManager.perform(new ClearLayerAction(layerManager, layerIndex));
    }
    
    void addLayer(const juce::String& name)
    {
        undoManager.perform(new AddLayerAction(layerManager, name));
    }
    
    void removeLayer(int layerIndex)
    {
        undoManager.perform(new RemoveLayerAction(layerManager, layerIndex));
    }
    
    void changeLayerProperty(int layerIndex, 
                             ChangeLayerPropertyAction::PropertyType property,
                             const juce::var& value)
    {
        undoManager.perform(new ChangeLayerPropertyAction(layerManager, layerIndex, property, value));
    }
    
    // Undo/Redo operations
    bool canUndo() const { return undoManager.canUndo(); }
    bool canRedo() const { return undoManager.canRedo(); }
    
    juce::String getUndoDescription() const { return undoManager.getUndoDescription(); }
    juce::String getRedoDescription() const { return undoManager.getRedoDescription(); }
    
    bool undo() { return undoManager.undo(); }
    bool redo() { return undoManager.redo(); }
    
    void clearUndoHistory() { undoManager.clearUndoHistory(); }
    
    // Begin/end action groups for complex operations
    void beginNewTransaction(const juce::String& name = {})
    {
        undoManager.beginNewTransaction(name);
    }
    
    // Get the underlying JUCE UndoManager for advanced operations
    juce::UndoManager& getUndoManager() { return undoManager; }
    
private:
    LayerManager* layerManager;
    juce::UndoManager undoManager;
};