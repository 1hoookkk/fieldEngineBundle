#include "PluginEditor.h"
#include "ui/XtremeLeadLookAndFeel.h"

MorphEngineEditor::MorphEngineEditor(MorphEngineProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    // Set EMU Xtreme Lead look and feel
    setLookAndFeel(&lookAndFeel);

    // Window size - EMU hardware proportions
    setSize(600, 400);

    // Configure main display
    addAndMakeVisible(mainDisplay);
    mainDisplay.setText("morphEngine", true);

    // Configure parameter display
    addAndMakeVisible(paramDisplay);
    paramDisplay.setText("Ready", true);

    // Setup encoders with labels
    auto setupEncoder = [&](MorphEngineUI::XtremeEncoder& encoder, juce::Label& label,
                           const juce::String& text) {
        addAndMakeVisible(encoder);
        addAndMakeVisible(label);
        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setColour(juce::Label::textColourId, MorphEngineUI::XtremeColors::textSilkscreen);
    };

    setupEncoder(morphKnob, morphLabel, "MORPH");
    setupEncoder(resonanceKnob, resonanceLabel, "RESONANCE");
    setupEncoder(brightnessKnob, brightnessLabel, "BRIGHTNESS");
    setupEncoder(driveKnob, driveLabel, "DRIVE");
    setupEncoder(hardnessKnob, hardnessLabel, "HARDNESS");
    setupEncoder(mixKnob, mixLabel, "MIX");

    // Setup linear sliders for style and quality
    addAndMakeVisible(styleSlider);
    addAndMakeVisible(styleLabel);
    styleSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    styleSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    styleLabel.setText("STYLE", juce::dontSendNotification);
    styleLabel.setJustificationType(juce::Justification::centredLeft);
    styleLabel.setColour(juce::Label::textColourId, MorphEngineUI::XtremeColors::textSilkscreen);

    addAndMakeVisible(qualitySlider);
    addAndMakeVisible(qualityLabel);
    qualitySlider.setSliderStyle(juce::Slider::LinearHorizontal);
    qualitySlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    qualityLabel.setText("QUALITY", juce::dontSendNotification);
    qualityLabel.setJustificationType(juce::Justification::centredLeft);
    qualityLabel.setColour(juce::Label::textColourId, MorphEngineUI::XtremeColors::textSilkscreen);

    // Z-plane display
    addAndMakeVisible(zplaneDisplay);

    // VU meters
    addAndMakeVisible(leftMeter);
    addAndMakeVisible(rightMeter);

    // Create attachments
    morphAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "morph", morphKnob);
    resonanceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "resonance", resonanceKnob);
    brightnessAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "brightness", brightnessKnob);
    driveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "drive", driveKnob);
    hardnessAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "hardness", hardnessKnob);
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "mix", mixKnob);
    styleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "style", styleSlider);
    qualityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "quality", qualitySlider);

    // Start timer for display updates
    startTimerHz(30);
}

MorphEngineEditor::~MorphEngineEditor()
{
    setLookAndFeel(nullptr);
}

void MorphEngineEditor::paint(juce::Graphics& g)
{
    // EMU chassis background
    g.fillAll(MorphEngineUI::XtremeColors::chassisBlack);

    // Draw panel sections
    auto bounds = getLocalBounds();

    // Top panel area with raised edge
    auto topPanel = bounds.removeFromTop(80);
    g.setColour(MorphEngineUI::XtremeColors::metalPanel);
    g.fillRect(topPanel.reduced(2));

    // Raised edge effect
    g.setColour(MorphEngineUI::XtremeColors::metalHighlight);
    g.drawRect(topPanel.reduced(1), 1);

    // Control panel area
    auto controlPanel = bounds.removeFromTop(200);
    g.setColour(MorphEngineUI::XtremeColors::chassisGrey);
    g.fillRect(controlPanel.reduced(2));

    // Bottom panel for meters
    g.setColour(MorphEngineUI::XtremeColors::metalPanel);
    g.fillRect(bounds.reduced(2));

    // EMU logo area (top left)
    g.setColour(MorphEngineUI::XtremeColors::lcdAmber);
    g.setFont(juce::Font("Arial Black", 16.0f, juce::Font::bold));
    g.drawText("EMU", 10, 5, 50, 20, juce::Justification::centred);

    // Model name (top right)
    g.setColour(MorphEngineUI::XtremeColors::textSilkscreen);
    g.setFont(juce::Font("Arial", 12.0f, juce::Font::plain));
    g.drawText("morphEngine Z-Plane", getWidth() - 150, 8, 140, 20, juce::Justification::centredRight);
}

void MorphEngineEditor::resized()
{
    auto bounds = getLocalBounds();

    // Top section - displays
    auto topSection = bounds.removeFromTop(80);
    topSection.reduce(10, 10);

    // Main display in center
    auto displayArea = topSection.removeFromTop(30);
    mainDisplay.setBounds(displayArea.removeFromLeft(200).withTrimmedLeft(60));

    // Param display on right
    paramDisplay.setBounds(displayArea.removeFromRight(180));

    // Main control section
    auto controlSection = bounds.removeFromTop(200);
    controlSection.reduce(20, 10);

    // Top row - Style and Quality sliders
    auto sliderRow = controlSection.removeFromTop(40);

    auto styleArea = sliderRow.removeFromLeft(getWidth() / 2 - 20);
    styleLabel.setBounds(styleArea.removeFromLeft(60));
    styleSlider.setBounds(styleArea.reduced(5, 10));

    auto qualityArea = sliderRow;
    qualityLabel.setBounds(qualityArea.removeFromLeft(60));
    qualitySlider.setBounds(qualityArea.reduced(5, 10));

    // Main encoders in 2x3 grid
    controlSection.removeFromTop(10);
    auto encoderSection = controlSection.removeFromTop(140);

    // Calculate encoder size
    const int encoderSize = 80;
    const int labelHeight = 15;
    const int spacing = (getWidth() - (3 * encoderSize)) / 4;

    // Top row of encoders
    auto topRow = encoderSection.removeFromTop(encoderSection.getHeight() / 2);
    int xPos = spacing;

    morphLabel.setBounds(xPos, topRow.getY(), encoderSize, labelHeight);
    morphKnob.setBounds(xPos, topRow.getY() + labelHeight, encoderSize, encoderSize - labelHeight);
    xPos += encoderSize + spacing;

    resonanceLabel.setBounds(xPos, topRow.getY(), encoderSize, labelHeight);
    resonanceKnob.setBounds(xPos, topRow.getY() + labelHeight, encoderSize, encoderSize - labelHeight);
    xPos += encoderSize + spacing;

    brightnessLabel.setBounds(xPos, topRow.getY(), encoderSize, labelHeight);
    brightnessKnob.setBounds(xPos, topRow.getY() + labelHeight, encoderSize, encoderSize - labelHeight);

    // Bottom row of encoders
    xPos = spacing;
    auto bottomRow = encoderSection;

    driveLabel.setBounds(xPos, bottomRow.getY(), encoderSize, labelHeight);
    driveKnob.setBounds(xPos, bottomRow.getY() + labelHeight, encoderSize, encoderSize - labelHeight);
    xPos += encoderSize + spacing;

    hardnessLabel.setBounds(xPos, bottomRow.getY(), encoderSize, labelHeight);
    hardnessKnob.setBounds(xPos, bottomRow.getY() + labelHeight, encoderSize, encoderSize - labelHeight);
    xPos += encoderSize + spacing;

    mixLabel.setBounds(xPos, bottomRow.getY(), encoderSize, labelHeight);
    mixKnob.setBounds(xPos, bottomRow.getY() + labelHeight, encoderSize, encoderSize - labelHeight);

    // Bottom section - Z-plane display and meters
    auto bottomSection = bounds.reduced(10);

    // Z-plane display on left
    zplaneDisplay.setBounds(bottomSection.removeFromLeft(250));

    // VU meters on right
    bottomSection.removeFromLeft(20);
    auto meterArea = bottomSection.removeFromRight(60);
    leftMeter.setBounds(meterArea.removeFromLeft(25));
    meterArea.removeFromLeft(10);
    rightMeter.setBounds(meterArea);
}

void MorphEngineEditor::timerCallback()
{
    updateDisplays();

    // Update Z-plane display
    float morph = processor.apvts.getRawParameterValue("morph")->load();
    float resonance = processor.apvts.getRawParameterValue("resonance")->load();
    float brightness = processor.apvts.getRawParameterValue("brightness")->load();
    zplaneDisplay.setFilterState(morph, resonance, brightness);

    // Update meters (placeholder - would get actual levels from processor)
    leftMeter.setLevel(0.7f);
    rightMeter.setLevel(0.65f);
}

void MorphEngineEditor::updateDisplays()
{
    // Update parameter display with current focused parameter
    // This is a simplified version - in production would track which control has focus
    float morphValue = processor.apvts.getRawParameterValue("morph")->load();
    paramDisplay.setText("Morph: " + juce::String(morphValue, 2), true);
}

juce::String MorphEngineEditor::getStyleName(float value)
{
    int styleIndex = (int)(value * 3.99f); // 0-3
    const char* styles[] = { "Velvet", "Air", "Focus", "Secret" };
    return styles[styleIndex];
}

juce::String MorphEngineEditor::getQualityName(float value)
{
    return value < 0.5f ? "Normal" : "HQ";
}

// Z-plane display implementation
void MorphEngineEditor::ZPlaneDisplay::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Background
    g.setColour(MorphEngineUI::XtremeColors::lcdBackground);
    g.fillRoundedRectangle(bounds, 4.0f);

    // Border
    g.setColour(MorphEngineUI::XtremeColors::metalPanel);
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);

    // Grid
    g.setColour(MorphEngineUI::XtremeColors::lcdAmber.withAlpha(0.2f));

    // Vertical lines
    for (int i = 1; i < 10; ++i)
    {
        float x = bounds.getX() + (bounds.getWidth() * i / 10.0f);
        g.drawVerticalLine((int)x, bounds.getY() + 2, bounds.getBottom() - 2);
    }

    // Horizontal lines
    for (int i = 1; i < 10; ++i)
    {
        float y = bounds.getY() + (bounds.getHeight() * i / 10.0f);
        g.drawHorizontalLine((int)y, bounds.getX() + 2, bounds.getRight() - 2);
    }

    // Draw unit circle
    auto center = bounds.getCentre();
    auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.35f;

    g.setColour(MorphEngineUI::XtremeColors::lcdAmber.withAlpha(0.5f));
    g.drawEllipse(center.x - radius, center.y - radius, radius * 2, radius * 2, 1.0f);

    // Draw poles based on morph/resonance
    float poleRadius = radius * (0.3f + currentResonance * 0.6f);
    float poleAngle = currentMorph * juce::MathConstants<float>::pi;

    auto pole1 = center.getPointOnCircumference(poleRadius, poleAngle);
    auto pole2 = center.getPointOnCircumference(poleRadius, -poleAngle);

    // Pole markers
    g.setColour(MorphEngineUI::XtremeColors::lcdAmberBright);
    g.fillEllipse(pole1.x - 4, pole1.y - 4, 8, 8);
    g.fillEllipse(pole2.x - 4, pole2.y - 4, 8, 8);

    // Draw zeros
    float zeroOffset = 50.0f * currentBrightness;
    g.setColour(MorphEngineUI::XtremeColors::ledBlue);
    g.drawEllipse(center.x - radius - zeroOffset - 3, center.y - 3, 6, 6, 2.0f);
    g.drawEllipse(center.x + radius + zeroOffset - 3, center.y - 3, 6, 6, 2.0f);

    // Label
    g.setColour(MorphEngineUI::XtremeColors::textSilkscreen);
    g.setFont(10.0f);
    g.drawText("Z-PLANE", bounds.reduced(4), juce::Justification::topLeft);
}

void MorphEngineEditor::ZPlaneDisplay::setFilterState(float morph, float resonance, float brightness)
{
    if (currentMorph != morph || currentResonance != resonance || currentBrightness != brightness)
    {
        currentMorph = morph;
        currentResonance = resonance;
        currentBrightness = brightness;
        repaint();
    }
}

// VU Meter implementation
void MorphEngineEditor::VUMeter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Background
    g.setColour(MorphEngineUI::XtremeColors::buttonInset);
    g.fillRect(bounds);

    // Meter segments (EMU style LED meter)
    const int numSegments = 12;
    float segmentHeight = bounds.getHeight() / numSegments;

    int litSegments = (int)(level * numSegments);

    for (int i = 0; i < numSegments; ++i)
    {
        float y = bounds.getBottom() - (i + 1) * segmentHeight;
        auto segmentBounds = juce::Rectangle<float>(bounds.getX(), y, bounds.getWidth(), segmentHeight - 1);

        if (i < litSegments)
        {
            // Choose color based on level
            if (i < 8)
                g.setColour(MorphEngineUI::XtremeColors::meterGreen);
            else if (i < 10)
                g.setColour(MorphEngineUI::XtremeColors::meterYellow);
            else
                g.setColour(MorphEngineUI::XtremeColors::meterRed);
        }
        else
        {
            g.setColour(MorphEngineUI::XtremeColors::buttonInset.brighter(0.2f));
        }

        g.fillRect(segmentBounds);
    }

    // Border
    g.setColour(MorphEngineUI::XtremeColors::metalHighlight);
    g.drawRect(bounds, 1.0f);
}