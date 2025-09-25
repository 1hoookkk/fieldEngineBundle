#include "ViralEditor.h"

ViralEditor::ViralEditor(FieldEngineFXProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(560, 400);

    // Precision controls
    morphSlider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
    intensitySlider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
    driveSlider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
    mixSlider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
    movementRateSlider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);

    // FabFilter-style dropdowns
    soloCombo = std::make_unique<juce::ComboBox>();
    pairCombo = std::make_unique<juce::ComboBox>();
    syncCombo = std::make_unique<juce::ComboBox>();

    // Precision parameter ranges for sound design
    morphSlider->setRange(0.0, 1.0, 0.0001);
    morphSlider->setValue(0.5);
    morphSlider->onValueChange = [this]() {
        audioProcessor.getAPVTS().getParameter("MORPH")->setValueNotifyingHost((float)morphSlider->getValue());
    };

    intensitySlider->setRange(0.0, 1.0, 0.001);
    intensitySlider->setValue(0.758);
    intensitySlider->onValueChange = [this]() {
        audioProcessor.getAPVTS().getParameter("intensity")->setValueNotifyingHost((float)intensitySlider->getValue());
    };

    driveSlider->setRange(0.1, 2.0, 0.01);
    driveSlider->setValue(0.8);
    driveSlider->onValueChange = [this]() {
        audioProcessor.getAPVTS().getParameter("DRIVE")->setValueNotifyingHost((float)driveSlider->getValue());
    };

    mixSlider->setRange(0.0, 1.0, 0.001);
    mixSlider->setValue(1.0);
    mixSlider->onValueChange = [this]() {
        audioProcessor.getAPVTS().getParameter("mix")->setValueNotifyingHost((float)mixSlider->getValue());
    };

    movementRateSlider->setRange(0.01, 20.0, 0.001);
    movementRateSlider->setValue(0.05);
    movementRateSlider->onValueChange = [this]() {
        audioProcessor.getAPVTS().getParameter("movementRate")->setValueNotifyingHost((float)movementRateSlider->getValue());
    };

    // Setup FabFilter-style dropdowns
    soloCombo->addItem("Off", 1);
    soloCombo->addItem("Wet", 2);
    soloCombo->addItem("Dry", 3);
    soloCombo->addItem("Diff", 4);
    soloCombo->setSelectedId(1);
    soloCombo->onChange = [this]() {
        audioProcessor.getAPVTS().getParameter("solo")->setValueNotifyingHost((float)(soloCombo->getSelectedId() - 1));
    };

    pairCombo->addItem("Vowel", 1);
    pairCombo->addItem("Bell", 2);
    pairCombo->addItem("Low", 3);
    pairCombo->setSelectedId(1);
    pairCombo->onChange = [this]() {
        audioProcessor.getAPVTS().getParameter("pair")->setValueNotifyingHost((float)(pairCombo->getSelectedId() - 1));
    };

    syncCombo->addItem("Free", 1);
    syncCombo->addItem("1/4", 2);
    syncCombo->addItem("1/8", 3);
    syncCombo->addItem("1/16", 4);
    syncCombo->addItem("1/32", 5);
    syncCombo->setSelectedId(1);
    syncCombo->onChange = [this]() {
        audioProcessor.getAPVTS().getParameter("sync")->setValueNotifyingHost((float)(syncCombo->getSelectedId() - 1));
    };

    addAndMakeVisible(*morphSlider);
    addAndMakeVisible(*intensitySlider);
    addAndMakeVisible(*driveSlider);
    addAndMakeVisible(*mixSlider);
    addAndMakeVisible(*movementRateSlider);
    addAndMakeVisible(*soloCombo);
    addAndMakeVisible(*pairCombo);
    addAndMakeVisible(*syncCombo);

    // Initialize spectrum colors - high contrast palette
    for (int i = 0; i < 32; ++i) {
        float hue = (float)i / 32.0f;
        spectrum[i].color = juce::Colour::fromHSV(hue, 0.9f, 1.0f, 1.0f);
    }

    // Look and feel and cartography view
    setLookAndFeel(&feLook);
    cartography = std::make_unique<CartographyView>();
    addAndMakeVisible(*cartography);

    startTimerHz(60); // Smooth 60fps visuals
}

ViralEditor::~ViralEditor()
{
    setLookAndFeel(nullptr);
}

void ViralEditor::paint(juce::Graphics& g)
{
    // Pure black background
    g.fillAll(juce::Colours::black);

    // Get current morph and intensity for visuals
    morphValue = morphSlider->getValue();
    intensityValue = intensitySlider->getValue();

    // Draw reactive spectrum visualization
    auto top = getLocalBounds().removeFromTop(20);
    juce::ignoreUnused(top);
    auto spectrumBounds = getLocalBounds().removeFromTop(120).reduced(8);
    drawSpectrum(g, spectrumBounds);

    // Draw filter response curve
    auto responseBounds = getLocalBounds().removeFromBottom(72).reduced(8);
    drawFilterResponse(g, responseBounds);

    // Precision control labels in high contrast
    g.setColour(juce::Colours::white);
    g.setFont(9.0f);
    g.drawText("DRIVE", driveSlider->getBounds().translated(0, -12), juce::Justification::left);
    g.drawText("FOCUS", morphSlider->getBounds().translated(0, -12), juce::Justification::left);
    g.drawText("CONTOUR", intensitySlider->getBounds().translated(0, -12), juce::Justification::left);
    g.drawText("MIX", mixSlider->getBounds().translated(0, -12), juce::Justification::left);
    g.drawText("RATE", movementRateSlider->getBounds().translated(0, -12), juce::Justification::left);

    // Dropdown labels
    g.drawText("SOLO", soloCombo->getBounds().translated(0, -12), juce::Justification::left);
    g.drawText("PAIR", pairCombo->getBounds().translated(0, -12), juce::Justification::left);
    g.drawText("SYNC", syncCombo->getBounds().translated(0, -12), juce::Justification::left);

    // Tagline and brand
    g.setColour(juce::Colours::white);
    g.setFont(10.0f);
    g.drawText("engineLabs  //  fieldEngine", 8, 6, 220, 14, juce::Justification::left);
    g.drawText("anything = music", getWidth() - 160, getHeight() - 18, 152, 12, juce::Justification::right);
}

void ViralEditor::resized()
{
    setSize(560, 400);

    auto bounds = getLocalBounds();
    auto header = bounds.removeFromTop(20);
    juce::ignoreUnused(header);
    auto viz = bounds.removeFromTop(140);
    if (cartography) cartography->setBounds(viz.reduced(8));
    bounds.removeFromBottom(72);

    auto controlHeight = 24;
    auto comboHeight = 20;
    auto spacing = 8;
    auto margin = 16;

    // Left column - precision sliders
    auto leftColumn = bounds.removeFromLeft(320).reduced(margin, 0);

    driveSlider->setBounds(leftColumn.removeFromTop(controlHeight));
    leftColumn.removeFromTop(spacing);

    morphSlider->setBounds(leftColumn.removeFromTop(controlHeight));
    leftColumn.removeFromTop(spacing);

    intensitySlider->setBounds(leftColumn.removeFromTop(controlHeight));
    leftColumn.removeFromTop(spacing);

    mixSlider->setBounds(leftColumn.removeFromTop(controlHeight));
    leftColumn.removeFromTop(spacing);

    movementRateSlider->setBounds(leftColumn.removeFromTop(controlHeight));

    // Right column - dropdowns
    auto rightColumn = bounds.reduced(margin, 0);

    soloCombo->setBounds(rightColumn.removeFromTop(comboHeight));
    rightColumn.removeFromTop(spacing);

    pairCombo->setBounds(rightColumn.removeFromTop(comboHeight));
    rightColumn.removeFromTop(spacing);

    syncCombo->setBounds(rightColumn.removeFromTop(comboHeight));
}

void ViralEditor::timerCallback()
{
    // Drain audio telemetry and update cartography
    float monoBuf[1024]; int n = 0;
    audioProcessor.drainTelemetry(monoBuf, 1024, n);
    if (cartography && n > 0) cartography->pushMonoSamples(std::span<const float>(monoBuf, (size_t)n));

    float rmsL, rmsR, peakL, peakR, mx, my; bool clipped;
    audioProcessor.getMorphTelemetry(rmsL, rmsR, peakL, peakR, mx, my, clipped);
    if (cartography) {
        fe::MorphEngine::Telemetry t{rmsL, rmsR, peakL, peakR, mx, my, clipped};
        cartography->setTelemetry(t);
    }

    // Mirror public controls to cartography params
    if (cartography) {
        cartography->setDriveDb(juce::jmap((float)audioProcessor.getAPVTS().getRawParameterValue("DRIVE")->load(), 0.1f, 2.0f, -12.0f, 18.0f));
        cartography->setFocus01(audioProcessor.getAPVTS().getRawParameterValue("MORPH")->load());
        cartography->setContour(audioProcessor.getAPVTS().getRawParameterValue("intensity")->load() * 2.0f - 1.0f);
    }

    updateVisuals();
    repaint();
}

void ViralEditor::updateVisuals()
{
    pulsePhase += 0.1f;
    if (pulsePhase > juce::MathConstants<float>::twoPi)
        pulsePhase -= juce::MathConstants<float>::twoPi;

    // Update spectrum bars based on morph position and intensity
    for (int i = 0; i < 32; ++i) {
        float freq = (float)i / 32.0f;

        // Create filter-like response based on morph position
        float distance = std::abs(freq - morphValue);
        float response = intensityValue * std::exp(-distance * 8.0f);

        // Add some movement
        response += 0.1f * std::sin(pulsePhase + i * 0.2f);

        spectrum[i].targetEnergy = juce::jlimit(0.0f, 1.0f, response);
        spectrum[i].energy = spectrum[i].energy * 0.7f + spectrum[i].targetEnergy * 0.3f;
    }
}

void ViralEditor::drawSpectrum(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    int barWidth = bounds.getWidth() / 32;

    for (int i = 0; i < 32; ++i) {
        auto barBounds = juce::Rectangle<int>(bounds.getX() + i * barWidth, bounds.getY(),
                                             barWidth - 1, bounds.getHeight());

        float energy = spectrum[i].energy;
        int barHeight = (int)(energy * bounds.getHeight());

        // High contrast colors based on frequency position and morph
        float hue = ((float)i / 32.0f + morphValue * 0.5f);
        while (hue > 1.0f) hue -= 1.0f;

        juce::Colour color = juce::Colour::fromHSV(hue, 0.8f, energy, 1.0f);

        g.setColour(color);
        g.fillRect(barBounds.getX(), barBounds.getBottom() - barHeight,
                  barBounds.getWidth(), barHeight);
    }
}

void ViralEditor::drawFilterResponse(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    juce::Path responsePath;

    // Draw a filter response curve that changes with morph
    int numPoints = bounds.getWidth();
    bool firstPoint = true;

    for (int x = 0; x < numPoints; ++x) {
        float freq = (float)x / numPoints;

        // Create filter response based on morph position
        float distance = std::abs(freq - morphValue);
        float response = intensityValue * std::exp(-distance * 6.0f);
        response = juce::jlimit(0.0f, 1.0f, response);

        float y = bounds.getBottom() - response * bounds.getHeight();

        if (firstPoint) {
            responsePath.startNewSubPath(bounds.getX() + x, y);
            firstPoint = false;
        } else {
            responsePath.lineTo(bounds.getX() + x, y);
        }
    }

    // Draw the response curve
    g.setColour(juce::Colours::white.withAlpha(0.8f));
    g.strokePath(responsePath, juce::PathStrokeType(2.0f));

    // Fill under curve with gradient
    responsePath.lineTo(bounds.getRight(), bounds.getBottom());
    responsePath.lineTo(bounds.getX(), bounds.getBottom());
    responsePath.closeSubPath();

    juce::ColourGradient gradient(juce::Colours::white.withAlpha(0.3f),
                                 bounds.getX(), bounds.getY(),
                                 juce::Colours::transparentBlack,
                                 bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill(gradient);
    g.fillPath(responsePath);
}