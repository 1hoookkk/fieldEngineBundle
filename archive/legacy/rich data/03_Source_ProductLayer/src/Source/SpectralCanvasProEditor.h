#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "SpectralCanvasProAudioProcessor.h"
#include "GUI/CanvasComponent.h"
#include "GUI/MinimalTopStrip.h"
#include "GUI/PerfHUD.h"
// SpectrogramComponent removed - CanvasComponent handles live spectrum display
#include "GUI/ToastManager.h"
#include "GUI/MiniHUD.h"
#include "GUI/components/MeterView.h"
#include "Core/SampleLoaderService.h"

/**
 * Phase 2-3 Minimal UI Editor
 * 
 * STRIPPED DOWN VERSION:
 * - MinimalTopStrip for essential controls only
 * - Full-bleed CanvasComponent for painting
 * - NO fancy panels until Phase 4
 * - RT-safe with <5ms paint-to-audio latency
 */
class SpectralCanvasProEditor : public juce::AudioProcessorEditor,
                              public juce::AudioProcessorValueTreeState::Listener,
                              public juce::Timer
{
public:
    SpectralCanvasProEditor(SpectralCanvasProAudioProcessor&);
    ~SpectralCanvasProEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;
    void timerCallback() override;
    
    // Parameter listener for HUD toggle
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    
    // Access to loading infrastructure for other components
    SampleLoaderService* getSampleLoader() const { return sampleLoader.get(); }
    ToastManager* getToastManager() const { return toastManager.get(); }

private:
    SpectralCanvasProAudioProcessor& audioProcessor;
    
    // Phase 2-3 Minimal UI Components
    std::unique_ptr<CanvasComponent> canvasComponent;
    std::unique_ptr<MinimalTopStrip> topStrip;
    std::unique_ptr<MeterView> meterView;
    
    // Phase 5 Performance HUD
    std::unique_ptr<PerfHUD> perfHUD;
    MiniHUD miniHud;
    
    // Sample loading infrastructure
    std::unique_ptr<SampleLoaderService> sampleLoader;
    std::unique_ptr<ToastManager> toastManager;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectralCanvasProEditor)
};