#pragma once

#include <JuceHeader.h>
#include <memory>

class CanvasPanel : public juce::Component,
                    public juce::FileDragAndDropTarget
{
public:
    CanvasPanel();
    ~CanvasPanel() override = default;

    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;

    // FileDragAndDropTarget overrides
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    // Image loading and management
    void loadImage(const juce::File& imageFile);
    void clearImage();

    // Get brightness at normalized position (0-1)
    float getBrightnessAt(float normX, float normY) const;

private:
    // Image data
    juce::Image currentImage;
    juce::File currentImageFile;

    // Display state
    bool hasImage{ false };
    juce::Rectangle<float> imageDisplayBounds;

    // Placeholder text
    std::unique_ptr<juce::Label> placeholderLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CanvasPanel)
};