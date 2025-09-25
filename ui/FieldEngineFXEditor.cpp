#include "FieldEngineFXEditor.h"

namespace FieldEngineFX {

FieldEngineFXEditor::FieldEngineFXEditor(FieldEngineFXProcessor& p, 
                                         juce::AudioProcessorValueTreeState& vts)
    : AudioProcessorEditor(&p), 
      audioProcessor(p), 
      valueTreeState(vts) {
    
    // Set initial size
    setSize(800, 600);
    setResizable(true, true);
    setResizeLimits(640, 480, 1920, 1080);
    
    // Initialize components
    setupComponents();
    attachParameters();
    applyAlienStyling();
    
    // Start animation
    startTimerHz(60);
    
    isInitialized = true;
}

FieldEngineFXEditor::~FieldEngineFXEditor() {
    stopTimer();
    detachParameters();
}

void FieldEngineFXEditor::paint(juce::Graphics& g) {
    // Render alien background
    backgroundRenderer.render(g, getLocalBounds(), animationTime);
    
    // Add vignette effect
    auto bounds = getLocalBounds().toFloat();
    juce::ColourGradient vignette(juce::Colours::transparentBlack, 
                                   bounds.getCentre(),
                                   juce::Colour(0x88000000),
                                   bounds.getTopLeft(), true);
    g.setGradientFill(vignette);
    g.fillRect(bounds);
}

void FieldEngineFXEditor::resized() {
    layout.calculate(getLocalBounds());
    
    // Position main display components
    if (zplaneGalaxy) {
        zplaneGalaxy->setBounds(layout.mainDisplay);
    }
    if (presetNebula) {
        presetNebula->setBounds(layout.mainDisplay);
        presetNebula->setVisible(currentMode == 1);
    }
    
    // Position control knobs
    auto controlArea = layout.controlPanel;
    auto knobSize = 80;
    auto spacing = 20;
    
    if (cutoffKnob) {
        cutoffKnob->setBounds(controlArea.removeFromLeft(knobSize)
                              .withHeight(knobSize));
        controlArea.removeFromLeft(spacing);
    }
    if (resonanceKnob) {
        resonanceKnob->setBounds(controlArea.removeFromLeft(knobSize)
                                 .withHeight(knobSize));
        controlArea.removeFromLeft(spacing);
    }
    if (morphKnob) {
        morphKnob->setBounds(controlArea.removeFromLeft(knobSize)
                             .withHeight(knobSize));
        controlArea.removeFromLeft(spacing);
    }
    
    // Position navigation
    auto navArea = layout.navigationBar;
    auto buttonSize = 40;
    
    if (galaxyModeButton) {
        galaxyModeButton->setBounds(navArea.removeFromLeft(buttonSize)
                                    .withHeight(buttonSize));
    }
    if (nebulaModeButton) {
        nebulaModeButton->setBounds(navArea.removeFromLeft(buttonSize)
                                    .withHeight(buttonSize));
    }
    if (matrixModeButton) {
        matrixModeButton->setBounds(navArea.removeFromLeft(buttonSize)
                                    .withHeight(buttonSize));
    }
}

void FieldEngineFXEditor::setupComponents() {
    // Create Z-plane galaxy
    zplaneGalaxy = std::make_unique<UI::ZPlaneGalaxy>();
    addAndMakeVisible(zplaneGalaxy.get());
    
    // Create preset nebula
    presetNebula = std::make_unique<UI::PresetNebula>();
    presetNebula->onPresetSelected = [this](const UI::PresetNebula::Preset& preset) {
        // Load preset into processor
        // audioProcessor.loadPreset(preset);
    };
    addAndMakeVisible(presetNebula.get());
    presetNebula->setVisible(false);
    
    // Create energy flow visualizer
    energyFlow = std::make_unique<UI::EnergyFlowVisualizer>();
    addAndMakeVisible(energyFlow.get());
    
    // Create modulation matrix
    modMatrix = std::make_unique<UI::ModulationMatrix>();
    addAndMakeVisible(modMatrix.get());
    modMatrix->setVisible(false);
    
    // Setup control knobs
    setupKnobs();
    
    // Setup navigation
    setupNavigation();
}

void FieldEngineFXEditor::setupKnobs() {
    // Cutoff knob
    cutoffKnob = std::make_unique<UI::ResonantBiomechanicalKnob>();
    cutoffKnob->setRange(20.0f, 20000.0f);
    cutoffKnob->setSkewFactor(0.3f);
    cutoffKnob->onValueChange = [this](float value) {
        // Update Z-plane visualization
        if (zplaneGalaxy) {
            // Update filter state
            // zplaneGalaxy->setCoefficients(...)
        }
    };
    addAndMakeVisible(cutoffKnob.get());
    
    // Resonance knob
    resonanceKnob = std::make_unique<UI::ResonantBiomechanicalKnob>();
    resonanceKnob->setRange(0.0f, 1.0f);
    resonanceKnob->setPrimaryColor(juce::Colour(0xffFF006E));
    addAndMakeVisible(resonanceKnob.get());
    
    // Morph knob
    morphKnob = std::make_unique<UI::BiomechanicalKnob>();
    morphKnob->setRange(0.0f, 1.0f);
    morphKnob->setOrganicComplexity(10);
    addAndMakeVisible(morphKnob.get());
    
    // Drive knob
    driveKnob = std::make_unique<UI::BiomechanicalKnob>();
    driveKnob->setRange(0.0f, 2.0f);
    driveKnob->setPrimaryColor(juce::Colour(0xffFFB700));
    addAndMakeVisible(driveKnob.get());
    
    // Mix knob
    mixKnob = std::make_unique<UI::BiomechanicalKnob>();
    mixKnob->setRange(0.0f, 1.0f);
    addAndMakeVisible(mixKnob.get());
}

void FieldEngineFXEditor::setupNavigation() {
    // Create mode buttons with alien glyphs
    auto createModeButton = [](const juce::String& name) {
        auto button = std::make_unique<juce::DrawableButton>(name, 
            juce::DrawableButton::ImageFitted);
        // Create alien glyph drawable
        // ... 
        return button;
    };
    
    galaxyModeButton = createModeButton("Galaxy");
    galaxyModeButton->onClick = [this] { transitionToMode(0); };
    addAndMakeVisible(galaxyModeButton.get());
    
    nebulaModeButton = createModeButton("Nebula");
    nebulaModeButton->onClick = [this] { transitionToMode(1); };
    addAndMakeVisible(nebulaModeButton.get());
    
    matrixModeButton = createModeButton("Matrix");
    matrixModeButton->onClick = [this] { transitionToMode(2); };
    addAndMakeVisible(matrixModeButton.get());
}

void FieldEngineFXEditor::applyAlienStyling() {
    // Set look and feel with custom alien theme
    setLookAndFeel(nullptr); // Would use custom AlienLookAndFeel
    
    // Configure tooltip window
    tooltipWindow = std::make_unique<juce::TooltipWindow>(this);
    tooltipWindow->setMillisecondsBeforeTipAppears(1000);
}

void FieldEngineFXEditor::timerCallback() {
    animationTime += 1.0f / 60.0f;
    
    // Update energy flow with audio data
    if (energyFlow) {
        // Get audio buffer from processor
        // energyFlow->pushAudioData(...)
    }
    
    // Animate mode transitions
    if (currentMode != getInterfaceMode()) {
        static float transitionProgress = 0.0f;
        transitionProgress += 0.05f;
        
        if (transitionProgress >= 1.0f) {
            currentMode = getInterfaceMode();
            transitionProgress = 0.0f;
        } else {
            animateModeTransition(transitionProgress);
        }
    }
}

void FieldEngineFXEditor::transitionToMode(int newMode) {
    if (newMode == currentMode) return;
    
    // Start transition animation
    setInterfaceMode(newMode);
    
    // Update component visibility
    zplaneGalaxy->setVisible(newMode == 0);
    presetNebula->setVisible(newMode == 1);
    modMatrix->setVisible(newMode == 2);
}

void FieldEngineFXEditor::setInterfaceMode(int mode) {
    currentMode = juce::jlimit(0, 2, mode);
}

void FieldEngineFXEditor::animateModeTransition(float progress) {
    // Smooth transition between modes
    float eased = std::sin(progress * juce::MathConstants<float>::halfPi);
    
    // Fade components
    if (zplaneGalaxy) {
        zplaneGalaxy->setAlpha(currentMode == 0 ? eased : 1.0f - eased);
    }
    if (presetNebula) {
        presetNebula->setAlpha(currentMode == 1 ? eased : 1.0f - eased);
    }
    if (modMatrix) {
        modMatrix->setAlpha(currentMode == 2 ? eased : 1.0f - eased);
    }
}

void FieldEngineFXEditor::attachParameters() {
    // Attach knobs to parameters
    // cutoffAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
    //     valueTreeState, "cutoff", *cutoffKnob);
}

void FieldEngineFXEditor::detachParameters() {
    // Clean up parameter attachments
}

void FieldEngineFXEditor::parameterChanged(const juce::String& parameterID, float newValue) {
    // Handle parameter updates from processor
    if (parameterID == "cutoff") {
        cutoffKnob->setValue(newValue, juce::dontSendNotification);
    } else if (parameterID == "resonance") {
        resonanceKnob->setValue(newValue, juce::dontSendNotification);
    }
}

// Background Renderer Implementation
void FieldEngineFXEditor::BackgroundRenderer::render(juce::Graphics& g, 
                                                      juce::Rectangle<int> bounds,
                                                      float time) {
    // Fill with void black
    g.fillAll(juce::Colour(0xff0A0E1B));
    
    // Draw layered effects
    drawStarfield(g, bounds, time);
    drawNebulaClouds(g, bounds, time);
    drawEnergyGrid(g, bounds, time);
}

void FieldEngineFXEditor::BackgroundRenderer::drawStarfield(juce::Graphics& g,
                                                             juce::Rectangle<int> bounds,
                                                             float time) {
    // Create parallax starfield
    juce::Random rng(42); // Fixed seed for consistent stars
    
    for (int layer = 0; layer < 3; ++layer) {
        float depth = 1.0f + layer * 0.5f;
        int starCount = 50 * (3 - layer);
        float brightness = 1.0f / depth;
        
        for (int i = 0; i < starCount; ++i) {
            float x = rng.nextFloat() * bounds.getWidth();
            float y = rng.nextFloat() * bounds.getHeight();
            
            // Parallax motion
            x += std::sin(time * 0.1f / depth) * 20.0f;
            y += std::cos(time * 0.15f / depth) * 15.0f;
            
            // Wrap around
            x = std::fmod(x + bounds.getWidth(), (float)bounds.getWidth());
            y = std::fmod(y + bounds.getHeight(), (float)bounds.getHeight());
            
            // Twinkling
            float twinkle = std::sin(time * rng.nextFloat() * 5.0f) * 0.3f + 0.7f;
            
            g.setColour(juce::Colour(0xffE8F4FF).withAlpha(brightness * twinkle * 0.7f));
            g.fillEllipse(x - 1, y - 1, 2, 2);
        }
    }
}

void FieldEngineFXEditor::BackgroundRenderer::drawNebulaClouds(juce::Graphics& g,
                                                                juce::Rectangle<int> bounds,
                                                                float time) {
    // Draw flowing nebula clouds
    g.setOpacity(0.2f);
    
    for (int i = 0; i < 3; ++i) {
        float phase = time * 0.05f + i * 2.0f;
        float x = bounds.getCentreX() + std::sin(phase) * bounds.getWidth() * 0.3f;
        float y = bounds.getCentreY() + std::cos(phase * 0.7f) * bounds.getHeight() * 0.2f;
        
        juce::ColourGradient nebula(
            juce::Colour(0xff6B5B95).withAlpha(0.0f),
            x, y,
            juce::Colour(0xff6B5B95).withAlpha(0.3f),
            x + 200, y + 200,
            true
        );
        
        g.setGradientFill(nebula);
        g.fillEllipse(x - 100, y - 100, 200, 200);
    }
}

void FieldEngineFXEditor::BackgroundRenderer::drawEnergyGrid(juce::Graphics& g,
                                                              juce::Rectangle<int> bounds,
                                                              float time) {
    // Draw perspective grid
    g.setColour(juce::Colour(0xff00FFB7).withAlpha(0.1f));
    
    int gridSize = 50;
    float perspective = 0.5f;
    
    // Vertical lines
    for (int x = 0; x < bounds.getWidth(); x += gridSize) {
        float topX = x;
        float bottomX = bounds.getCentreX() + (x - bounds.getCentreX()) * perspective;
        
        g.drawLine(topX, 0, bottomX, bounds.getHeight(), 0.5f);
    }
    
    // Horizontal lines with wave distortion
    for (int y = 0; y < bounds.getHeight(); y += gridSize) {
        juce::Path gridLine;
        gridLine.startNewSubPath(0, y);
        
        for (int x = 0; x < bounds.getWidth(); x += 10) {
            float wave = std::sin(x * 0.01f + time) * 5.0f * (y / (float)bounds.getHeight());
            gridLine.lineTo(x, y + wave);
        }
        
        g.strokePath(gridLine, juce::PathStrokeType(0.5f));
    }
}

// Layout Grid Implementation
void FieldEngineFXEditor::LayoutGrid::calculate(juce::Rectangle<int> totalBounds) {
    auto bounds = totalBounds.reduced(10);
    
    // Navigation bar at top
    navigationBar = bounds.removeFromTop(50);
    bounds.removeFromTop(10);
    
    // Main display area (golden ratio)
    int displayHeight = bounds.getHeight() * 0.618f;
    mainDisplay = bounds.removeFromTop(displayHeight);
    bounds.removeFromTop(10);
    
    // Bottom control area
    controlPanel = bounds.removeFromLeft(bounds.getWidth() * 0.7f);
    modulationSection = bounds;
}

} // namespace FieldEngineFX
