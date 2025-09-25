#include "SpectralCanvasProEditor.h"
#include "Core/Params.h"
#include "Core/DiagnosticLogger.h"
#include "Viz/backends/D3D11Renderer.h"

SpectralCanvasProEditor::SpectralCanvasProEditor(SpectralCanvasProAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    // Phase 2-3 Minimal UI: Top strip + full-bleed canvas
    
    // Canvas component (full-bleed background)
    canvasComponent = std::make_unique<CanvasComponent>(audioProcessor);
    canvasComponent->setParentEditor(this);
    addAndMakeVisible(*canvasComponent);
    
    // Minimal top strip with essential controls only
    topStrip = std::make_unique<MinimalTopStrip>(audioProcessor.apvts);
    addAndMakeVisible(*topStrip);
    
    // Add simple peak meters (UI thread only)
    meterView = std::make_unique<MeterView>();
    addAndMakeVisible(*meterView);
    meterView->start();

    // Add MiniHUD (compact textual status)
    addAndMakeVisible(miniHud);
    
    // Timer for HUD updates
    startTimerHz(30);
    
    // Enable keyboard focus for 'H' key toggle
    setWantsKeyboardFocus(true);
    
    // Minimal sizing - no complex layouts needed
    setSize(1200, 800);
    setResizable(true, true);
    setResizeLimits(800, 600, 2400, 1600);
}

SpectralCanvasProEditor::~SpectralCanvasProEditor()
{
    // Clean up sample loading infrastructure
    sampleLoader.reset();
    toastManager.reset();
    
    // Renderer removed for live insert mode
        
    // Remove parameter listener
    audioProcessor.getValueTreeState().removeParameterListener(Params::ParameterIDs::showPerfHud, this);
    
    perfHUD.reset();
    topStrip.reset();
    canvasComponent.reset();
}

void SpectralCanvasProEditor::paint(juce::Graphics& g)
{
    // Minimal dark background fallback
    g.fillAll(juce::Colour(0xff0a0a0f));
}

void SpectralCanvasProEditor::resized()
{
    auto bounds = getLocalBounds();
    
    // Top strip gets fixed height (36px), canvas fills remaining area
    auto topArea = bounds.removeFromTop(36);
    topStrip->setBounds(topArea);
    auto right = bounds.removeFromRight(60);
    canvasComponent->setBounds(bounds);
    if (meterView)
        meterView->setBounds(right);

    // Place MiniHUD at top-left below the strip
    miniHud.setBounds(8, topArea.getBottom() + 4, 360, 20);
    
    // Position HUD in top-right corner with margin
    if (perfHUD) {
        const int margin = 10;
        perfHUD->setTopRightPosition(getWidth() - margin, 50);
    }
    
    // Size toast manager to cover entire editor
    if (toastManager) {
        toastManager->setBounds(getLocalBounds());
    }
    
}

void SpectralCanvasProEditor::timerCallback()
{
    // Poll the processor for the latest snapshot of performance metrics and state
    SpectralCanvasProAudioProcessor::CanvasSnapshot snapshot;
    if (audioProcessor.getCanvasSnapshot(snapshot))
    {
        MiniHUDSnapshot hudSnap;
        hudSnap.sampleRate = (int)snapshot.sampleRate;
        hudSnap.blockSize = snapshot.blockSize;
        hudSnap.latencyMs = snapshot.metrics.medianLatencyMs;
        hudSnap.cpuPct = 0.0f; // Placeholder until available
        hudSnap.writing = snapshot.wroteAudioFlag;
        miniHud.setSnapshot(hudSnap);
    }

    // You could also update the input level LEDs on the top strip here
    // if you had a way to get live input levels from the processor.
}

bool SpectralCanvasProEditor::keyPressed(const juce::KeyPress& key)
{
    // Handle 'H' key for HUD toggle
    if (key.getKeyCode() == 'H' || key.getKeyCode() == 'h') {
        if (perfHUD) {
            // Toggle the parameter value (0.0 -> 1.0, 1.0 -> 0.0)
            auto* hudParam = audioProcessor.getValueTreeState().getParameter(Params::ParameterIDs::showPerfHud);
            if (hudParam != nullptr) {
                float currentValue = hudParam->getValue();
                hudParam->setValueNotifyingHost(currentValue < 0.5f ? 1.0f : 0.0f);
            }
        }
        return true;
    }
    
    // Pass other keys to canvas component for painting shortcuts
    if (canvasComponent) {
        return canvasComponent->keyPressed(key);
    }
    
    return false;
}

void SpectralCanvasProEditor::parameterChanged(const juce::String& parameterID, float newValue)
{
    if (parameterID == Params::ParameterIDs::showPerfHud && perfHUD) {
        // Update HUD visibility on main message thread
        juce::MessageManager::callAsync([this, newValue]() {
            if (perfHUD) {
                perfHUD->setVisible(newValue >= 0.5f);
            }
        });
    }
}
