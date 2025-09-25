#include "PremiumMorphUI.h"

// Debug flag for UI initialization tracking
constexpr bool kEnableUIDebug = true;

// UAD-inspired professional color palette
const juce::Colour PremiumMorphUI::backgroundDark   (0xFF1E1E1E);
const juce::Colour PremiumMorphUI::panelDark        (0xFF2A2A2A);
const juce::Colour PremiumMorphUI::accentBlue       (0xFF4A90E2);
const juce::Colour PremiumMorphUI::textWhite        (0xFFE0E0E0);
const juce::Colour PremiumMorphUI::textGrey         (0xFF888888);
const juce::Colour PremiumMorphUI::knobRing         (0xFF555555);

PremiumMorphUI::PremiumMorphUI (MorphEngineAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    if constexpr (kEnableUIDebug) { DBG("PremiumMorphUI: Starting construction"); }

    setSize (640, 400);

    if constexpr (kEnableUIDebug) { DBG("PremiumMorphUI: Setting up look and feel"); }
    setupLookAndFeel();

    if constexpr (kEnableUIDebug) { DBG("PremiumMorphUI: Creating controls"); }
    createControls();

    // Create spectrum display
    if constexpr (kEnableUIDebug) { DBG("PremiumMorphUI: Creating ResonanceLoom"); }
    spectrumDisplay = std::make_unique<ResonanceLoom>(processor);
    if constexpr (kEnableUIDebug) { DBG("PremiumMorphUI: ResonanceLoom created successfully"); }

    addAndMakeVisible(*spectrumDisplay);

    startTimerHz(30); // 30fps for smooth updates

    if constexpr (kEnableUIDebug) { DBG("PremiumMorphUI: Editor ready"); }
}

PremiumMorphUI::~PremiumMorphUI()
{
    stopTimer();
}

void PremiumMorphUI::setupLookAndFeel()
{
    // Configure clean, modern look and feel
    // Using system fonts for maximum compatibility
}

void PremiumMorphUI::createControls()
{
    // Title label
    titleLabel.setText ("morphEngine", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, textWhite);
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel);

    // Morph control
    morphSlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    morphSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    morphSlider.setRange (0.0, 1.0, 0.001);
    morphSlider.setColour (juce::Slider::rotarySliderFillColourId, accentBlue);
    morphSlider.setColour (juce::Slider::rotarySliderOutlineColourId, knobRing);
    morphSlider.setColour (juce::Slider::thumbColourId, accentBlue);
    addAndMakeVisible (morphSlider);

    morphLabel.setText ("MORPH", juce::dontSendNotification);
    morphLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    morphLabel.setColour (juce::Label::textColourId, textGrey);
    morphLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (morphLabel);

    // Resonance control
    resonanceSlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    resonanceSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    resonanceSlider.setRange (0.0, 1.0, 0.001);
    resonanceSlider.setColour (juce::Slider::rotarySliderFillColourId, accentBlue);
    resonanceSlider.setColour (juce::Slider::rotarySliderOutlineColourId, knobRing);
    addAndMakeVisible (resonanceSlider);

    resonanceLabel.setText ("RESONANCE", juce::dontSendNotification);
    resonanceLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    resonanceLabel.setColour (juce::Label::textColourId, textGrey);
    resonanceLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (resonanceLabel);

    // Mix control
    mixSlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    mixSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    mixSlider.setRange (0.0, 1.0, 0.001);
    mixSlider.setColour (juce::Slider::rotarySliderFillColourId, accentBlue);
    mixSlider.setColour (juce::Slider::rotarySliderOutlineColourId, knobRing);
    addAndMakeVisible (mixSlider);

    mixLabel.setText ("MIX", juce::dontSendNotification);
    mixLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    mixLabel.setColour (juce::Label::textColourId, textGrey);
    mixLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (mixLabel);

    // Drive control
    driveSlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    driveSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    driveSlider.setRange (0.0, 10.0, 0.01);
    driveSlider.setColour (juce::Slider::rotarySliderFillColourId, accentBlue);
    driveSlider.setColour (juce::Slider::rotarySliderOutlineColourId, knobRing);
    addAndMakeVisible (driveSlider);

    driveLabel.setText ("DRIVE", juce::dontSendNotification);
    driveLabel.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    driveLabel.setColour (juce::Label::textColourId, textGrey);
    driveLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (driveLabel);

    // Preset selector
    presetCombo.setTextWhenNothingSelected ("Select Preset");
    presetCombo.setColour (juce::ComboBox::backgroundColourId, panelDark);
    presetCombo.setColour (juce::ComboBox::textColourId, textWhite);
    presetCombo.setColour (juce::ComboBox::outlineColourId, knobRing);
    presetCombo.setColour (juce::ComboBox::arrowColourId, textGrey);

    // Add presets from processor
    for (int i = 0; i < processor.getNumPresets(); ++i)
    {
        presetCombo.addItem (processor.getPresetName(i), i + 1);
    }
    addAndMakeVisible (presetCombo);

    // Create parameter attachments
    morphAttachment     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "zplane.morph", morphSlider);
    resonanceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "zplane.resonance", resonanceSlider);
    mixAttachment       = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "style.mix", mixSlider);
    driveAttachment     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "drive.db", driveSlider);
}

void PremiumMorphUI::paint (juce::Graphics& g)
{
    drawBackground (g);
}

void PremiumMorphUI::drawBackground (juce::Graphics& g)
{
    // Fill with dark background
    g.fillAll (backgroundDark);

    // Draw main panel with subtle gradient
    auto mainArea = getLocalBounds().toFloat().reduced (8.0f);
    juce::ColourGradient gradient (
        panelDark.brighter(0.1f), mainArea.getX(), mainArea.getY(),
        panelDark.darker(0.1f), mainArea.getX(), mainArea.getBottom(),
        false);
    g.setGradientFill (gradient);
    g.fillRoundedRectangle (mainArea, 6.0f);

    // Draw subtle panel outline
    g.setColour (knobRing);
    g.drawRoundedRectangle (mainArea, 6.0f, 1.0f);

    // Draw section dividers
    g.setColour (knobRing.withAlpha(0.3f));

    // Divider between spectrum and controls
    float dividerY = headerArea.getBottom() + spectrumArea.getHeight() + 4;
    g.drawLine (20.0f, dividerY, getWidth() - 20.0f, dividerY, 1.0f);
}

void PremiumMorphUI::resized()
{
    auto bounds = getLocalBounds().reduced (12);

    // Header area
    headerArea = bounds.removeFromTop (40);
    titleLabel.setBounds (headerArea.removeFromLeft (200));
    presetCombo.setBounds (headerArea.removeFromRight (180).reduced (0, 8));

    bounds.removeFromTop (8); // Spacing

    // Spectrum display area
    spectrumArea = bounds.removeFromTop (120);
    spectrumDisplay->setBounds (spectrumArea);

    bounds.removeFromTop (16); // Spacing

    // Controls area
    controlsArea = bounds;

    // Layout knobs in a row
    int knobWidth = 80;
    int knobHeight = 100;
    int spacing = (controlsArea.getWidth() - (4 * knobWidth)) / 5;

    auto knobArea = controlsArea.withHeight (knobHeight);

    // Morph knob
    auto morphArea = knobArea.removeFromLeft (knobWidth).translated (spacing, 0);
    morphSlider.setBounds (morphArea.removeFromTop (knobWidth));
    morphLabel.setBounds (morphArea);

    // Resonance knob
    auto resArea = knobArea.removeFromLeft (knobWidth).translated (spacing, 0);
    resonanceSlider.setBounds (resArea.removeFromTop (knobWidth));
    resonanceLabel.setBounds (resArea);

    // Mix knob
    auto mixArea = knobArea.removeFromLeft (knobWidth).translated (spacing, 0);
    mixSlider.setBounds (mixArea.removeFromTop (knobWidth));
    mixLabel.setBounds (mixArea);

    // Drive knob
    auto driveArea = knobArea.removeFromLeft (knobWidth).translated (spacing, 0);
    driveSlider.setBounds (driveArea.removeFromTop (knobWidth));
    driveLabel.setBounds (driveArea);
}

void PremiumMorphUI::timerCallback()
{
    // Update any real-time displays
    repaint();
}