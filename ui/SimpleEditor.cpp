#include "SimpleEditor.h"
#include "../fx/FieldEngineFXProcessor.h"
#include "../shared/AsciiVisualizer.h"
#include "TemplePalette.h"

SimpleEditor::SimpleEditor(FieldEngineFXProcessor& p)
	: juce::AudioProcessorEditor(&p), audioProcessor(p)
{
	setOpaque(true);

	// Create and configure visualizer
	visualizer = std::make_unique<AsciiVisualizer>();
	addAndMakeVisible(visualizer.get());

	// Create JUCE sliders with MetaSynth/Temple aesthetic
	setupControls();

	// Initialize spectrum levels and start timer
	spectrumLevels.fill(0.0f);
	startTimer(50); // 20fps refresh rate

	// 900x650 window for full MetaSynth experience
	setSize(900, 650);
}

SimpleEditor::~SimpleEditor()
{
}

void SimpleEditor::setupControls()
{
	// Primary morphing controls (left side)
	morphSlider = std::make_unique<juce::Slider>(juce::Slider::Rotary, juce::Slider::TextBoxBelow);
	morphSlider->setRange(0.0, 1.0, 0.001);
	morphSlider->setValue(0.5);
	morphSlider->setColour(juce::Slider::thumbColourId, juce::Colour(0xFF00FFFF));
	morphSlider->setColour(juce::Slider::trackColourId, juce::Colour(0xFF003366));
	morphSlider->setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xFF00D8FF));
	morphSlider->setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xFFFFFF55));
	morphSlider->setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xFF000000));
	morphSlider->setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xFF00D8FF));
	addAndMakeVisible(morphSlider.get());

	intensitySlider = std::make_unique<juce::Slider>(juce::Slider::Rotary, juce::Slider::TextBoxBelow);
	intensitySlider->setRange(0.0, 1.0, 0.001);
	intensitySlider->setValue(0.4);
	intensitySlider->setColour(juce::Slider::thumbColourId, juce::Colour(0xFFFF00AA));
	intensitySlider->setColour(juce::Slider::trackColourId, juce::Colour(0xFF330033));
	intensitySlider->setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xFFFF00AA));
	intensitySlider->setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xFFFFFF55));
	intensitySlider->setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xFF000000));
	intensitySlider->setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xFFFF00AA));
	addAndMakeVisible(intensitySlider.get());

	driveSlider = std::make_unique<juce::Slider>(juce::Slider::Rotary, juce::Slider::TextBoxBelow);
	driveSlider->setRange(0.0, 24.0, 0.1);
	driveSlider->setValue(3.0);
	driveSlider->setColour(juce::Slider::thumbColourId, juce::Colour(0xFFFFFF55));
	driveSlider->setColour(juce::Slider::trackColourId, juce::Colour(0xFF333300));
	driveSlider->setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xFFFFFF55));
	driveSlider->setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xFFFFFF55));
	driveSlider->setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xFF000000));
	driveSlider->setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xFFFFFF55));
	addAndMakeVisible(driveSlider.get());

	// Modulation controls (right side)
	lfoRateSlider = std::make_unique<juce::Slider>(juce::Slider::Rotary, juce::Slider::TextBoxBelow);
	lfoRateSlider->setRange(0.02, 8.0, 0.01);
	lfoRateSlider->setValue(1.2);
	lfoRateSlider->setColour(juce::Slider::thumbColourId, juce::Colour(0xFF00D8FF));
	lfoRateSlider->setColour(juce::Slider::trackColourId, juce::Colour(0xFF002244));
	lfoRateSlider->setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xFF00D8FF));
	addAndMakeVisible(lfoRateSlider.get());

	lfoDepthSlider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
	lfoDepthSlider->setRange(0.0, 1.0, 0.001);
	lfoDepthSlider->setValue(0.15);
	lfoDepthSlider->setColour(juce::Slider::thumbColourId, juce::Colour(0xFFFF00AA));
	lfoDepthSlider->setColour(juce::Slider::trackColourId, juce::Colour(0xFF004466));
	addAndMakeVisible(lfoDepthSlider.get());

	envDepthSlider = std::make_unique<juce::Slider>(juce::Slider::Rotary, juce::Slider::TextBoxBelow);
	envDepthSlider->setRange(0.0, 1.0, 0.001);
	envDepthSlider->setValue(0.35);
	envDepthSlider->setColour(juce::Slider::thumbColourId, juce::Colour(0xFFFF00AA));
	envDepthSlider->setColour(juce::Slider::trackColourId, juce::Colour(0xFF002244));
	envDepthSlider->setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xFFFF00AA));
	addAndMakeVisible(envDepthSlider.get());

    mixSlider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
    mixSlider->setRange(0.0, 1.0, 0.001);
    mixSlider->setValue(1.0);
    mixSlider->setColour(juce::Slider::thumbColourId, juce::Colour(0xFF00FFFF));
    mixSlider->setColour(juce::Slider::trackColourId, juce::Colour(0xFF004466));
    addAndMakeVisible(mixSlider.get());

    // Wet % readout
    wetLabel = std::make_unique<juce::Label>();
    wetLabel->setJustificationType(juce::Justification::centredRight);
    wetLabel->setColour(juce::Label::textColourId, juce::Colour(0xFF55AAFF));
    wetLabel->setFont(juce::FontOptions("Consolas", 10.0f, juce::Font::plain));
    wetLabel->setText("100%", juce::dontSendNotification);
    wetLabel->setInterceptsMouseClicks(false, false);
    addAndMakeVisible(wetLabel.get());

    // Minimal pair selector (vowel/bell/low)
    pairBox = std::make_unique<juce::ComboBox>();
    pairBox->addItem("vowel", 1);
    pairBox->addItem("bell", 2);
    pairBox->addItem("low", 3);
    pairBox->setJustificationType(juce::Justification::centred);
    addAndMakeVisible(pairBox.get());
    pairAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.getAPVTS(), "pair", *pairBox);

    // CRT overlay toggle
    crtToggle = std::make_unique<juce::ToggleButton>("crt");
    crtToggle->setToggleState(true, juce::dontSendNotification);
    addAndMakeVisible(crtToggle.get());
    crtAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(audioProcessor.getAPVTS(), "crt", *crtToggle);

    // Solo effect toggle
    soloToggle = std::make_unique<juce::ToggleButton>("solo");
    soloToggle->setToggleState(false, juce::dontSendNotification);
    addAndMakeVisible(soloToggle.get());
    soloAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(audioProcessor.getAPVTS(), "solo", *soloToggle);
}

void SimpleEditor::drawModernPanel(juce::Graphics& g, juce::Rectangle<int> bounds, 
                                    const juce::String& title, juce::Colour accentColor)
{
    // Modern panel with subtle gradient background
    juce::ColourGradient panelGrad(juce::Colour(0xFF2a2a2a), bounds.getX(), bounds.getY(),
                                   juce::Colour(0xFF1f1f1f), bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill(panelGrad);
    g.fillRoundedRectangle(bounds.toFloat(), 8.0f);
    
    // Modern border with accent color
    g.setColour(accentColor.withAlpha(0.6f));
    g.drawRoundedRectangle(bounds.toFloat(), 8.0f, 2.0f);
    
    // Title bar
    auto titleBounds = bounds.removeFromTop(30);
    g.setColour(accentColor.withAlpha(0.2f));
    g.fillRoundedRectangle(titleBounds.toFloat(), 8.0f);
    
    // Title text
    g.setColour(accentColor);
    g.setFont(juce::FontOptions("Segoe UI", 12.0f, juce::Font::bold));
    g.drawText(title, titleBounds, juce::Justification::centred);
}

void SimpleEditor::paint(juce::Graphics& g)
{
	// Modern dark background with subtle gradient
	juce::ColourGradient bg(juce::Colour(0xFF1a1a1a), 0, 0, 
	                        juce::Colour(0xFF0f0f0f), 0, (float)getHeight(), false);
	g.setGradientFill(bg);
	g.fillAll();

	// Modern header with better proportions
	auto headerBounds = juce::Rectangle<int>(0, 0, getWidth(), 80);
	juce::ColourGradient headerGrad(juce::Colour(0xFF2a2a2a), 0, 0,
	                                juce::Colour(0xFF1f1f1f), 0, 80, false);
	g.setGradientFill(headerGrad);
	g.fillRect(headerBounds);
	
	// Subtle header border
	g.setColour(juce::Colour(0xFF404040));
	g.drawLine(0, 79, (float)getWidth(), 79, 1.0f);

    // Modern title with better typography
    g.setColour(juce::Colour(0xFF00d4ff));
    g.setFont(juce::FontOptions("Segoe UI", 32.0f, juce::Font::bold));
    g.drawText("FieldEngine", 30, 15, 400, 40, juce::Justification::left);
    
    // Subtitle
    g.setColour(juce::Colour(0xFF888888));
    g.setFont(juce::FontOptions("Segoe UI", 14.0f, juce::Font::plain));
    g.drawText("EMU Z-Plane Morphing Filter", 30, 50, 400, 25, juce::Justification::left);

	// Better panel layout with proper spacing
	int margin = 25;
	int panelSpacing = 20;
	int topY = 100;
	int panelHeight = 280;
	
	// Calculate panel widths dynamically
	int totalWidth = getWidth() - 2 * margin - 2 * panelSpacing;
	int leftWidth = 200;
	int rightWidth = 200;
	int centerWidth = totalWidth - leftWidth - rightWidth;
	
	// Left panel - Primary Controls
	auto leftPanel = juce::Rectangle<int>(margin, topY, leftWidth, panelHeight);
	drawModernPanel(g, leftPanel, "FILTER", juce::Colour(0xFF00d4ff));

	// Center panel - Visualizer  
	auto centerPanel = juce::Rectangle<int>(leftPanel.getRight() + panelSpacing, topY, 
	                                       centerWidth, panelHeight);
	drawModernPanel(g, centerPanel, "VISUALIZER", juce::Colour(0xFF00ff88));

    // Draw improved orbit display
    drawOrbitDisplay(g, centerPanel.reduced(15, 35));

	// Right panel - Modulation
	auto rightPanel = juce::Rectangle<int>(centerPanel.getRight() + panelSpacing, topY,
	                                      rightWidth, panelHeight);
	drawModernPanel(g, rightPanel, "MODULATION", juce::Colour(0xFFff6b00));

	// Bottom spectrum section with modern styling
	auto spectrumPanel = juce::Rectangle<int>(margin, topY + panelHeight + panelSpacing, 
	                                         getWidth() - 2*margin, 140);
	drawModernPanel(g, spectrumPanel, "SPECTRUM ANALYZER", juce::Colour(0xFFff0088));

	// Draw improved spectrum meters
	drawModernSpectrum(g, spectrumPanel.reduced(15, 35));

    // Modern parameter labels with better positioning
    g.setFont(juce::FontOptions("Segoe UI", 10.0f, juce::Font::plain));
    g.setColour(juce::Colour(0xFFaaaaaa));
    
    // Left panel labels
    g.drawText("MORPH", leftPanel.getX() + 20, leftPanel.getY() + 45, leftWidth - 40, 15, juce::Justification::centred);
    g.drawText("INTENSITY", leftPanel.getX() + 20, leftPanel.getY() + 125, leftWidth - 40, 15, juce::Justification::centred);
    g.drawText("DRIVE", leftPanel.getX() + 20, leftPanel.getY() + 205, leftWidth - 40, 15, juce::Justification::centred);
    
    // Right panel labels
    g.drawText("LFO RATE", rightPanel.getX() + 20, rightPanel.getY() + 45, rightWidth - 40, 15, juce::Justification::centred);
    g.drawText("LFO AMOUNT", rightPanel.getX() + 20, rightPanel.getY() + 105, rightWidth - 40, 15, juce::Justification::centred);
    g.drawText("ENV DEPTH", rightPanel.getX() + 20, rightPanel.getY() + 165, rightWidth - 40, 15, juce::Justification::centred);
    g.drawText("MIX", rightPanel.getX() + 20, rightPanel.getY() + 225, rightWidth - 40, 15, juce::Justification::centred);
}

void SimpleEditor::drawModernSpectrum(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Draw improved spectrum meters
	const int numBands = 8;
	const int meterSpacing = bounds.getWidth() / numBands;
	const int meterWidth = (int)(meterSpacing * 0.7f);
	const int meterHeight = bounds.getHeight() - 20;

	const juce::Colour meterColors[8] = {
		juce::Colour(0xFF0088ff), // Blue
		juce::Colour(0xFF00aaff), 
		juce::Colour(0xFF00ccff), 
		juce::Colour(0xFF00ffcc), // Cyan
		juce::Colour(0xFF88ff00), // Green
		juce::Colour(0xFFffcc00), // Yellow
		juce::Colour(0xFFff8800), // Orange
		juce::Colour(0xFFff0088)  // Pink
	};

	for (int i = 0; i < numBands; ++i)
	{
		int x = bounds.getX() + (i * meterSpacing) + (meterSpacing - meterWidth) / 2;
		int y = bounds.getY() + 10;

		// Meter background with rounded corners
		g.setColour(juce::Colour(0xFF111111));
		g.fillRoundedRectangle(x, y, meterWidth, meterHeight, 4.0f);
		
		g.setColour(juce::Colour(0xFF333333));
		g.drawRoundedRectangle(x, y, meterWidth, meterHeight, 4.0f, 1.0f);

		// Meter fill with gradient
		float level = spectrumLevels[i];
		int fillHeight = static_cast<int>(level * (meterHeight - 4));
		if (fillHeight > 4)
		{
		    juce::ColourGradient meterGrad(meterColors[i].brighter(0.3f), x, y + meterHeight - fillHeight,
		                                   meterColors[i], x, y + meterHeight, false);
		    g.setGradientFill(meterGrad);
			g.fillRoundedRectangle(x + 2, y + meterHeight - fillHeight, meterWidth - 4, fillHeight, 2.0f);
		}
	}
}

void SimpleEditor::resized()
{
	// Calculate layout based on new modern design
	int margin = 25;
	int panelSpacing = 20;
	int topY = 100;
	int panelHeight = 280;
	
	// Calculate panel widths dynamically
	int totalWidth = getWidth() - 2 * margin - 2 * panelSpacing;
	int leftWidth = 200;
	int rightWidth = 200;
	int centerWidth = totalWidth - leftWidth - rightWidth;
	
	// Left panel bounds
	auto leftPanel = juce::Rectangle<int>(margin, topY, leftWidth, panelHeight);
	
	// Position primary controls in left panel with better spacing
	int controlSize = 70;
	int controlSpacing = 80;
	int controlStartY = leftPanel.getY() + 60;
	int controlX = leftPanel.getX() + (leftWidth - controlSize) / 2;
	
	morphSlider->setBounds(controlX, controlStartY, controlSize, controlSize);
	intensitySlider->setBounds(controlX, controlStartY + controlSpacing, controlSize, controlSize);
	driveSlider->setBounds(controlX, controlStartY + 2 * controlSpacing, controlSize, controlSize);

	// Center panel for visualizer
	auto centerPanel = juce::Rectangle<int>(leftPanel.getRight() + panelSpacing, topY, 
	                                       centerWidth, panelHeight);
	if (visualizer)
		visualizer->setBounds(centerPanel.reduced(15, 35));

	// Right panel bounds
	auto rightPanel = juce::Rectangle<int>(centerPanel.getRight() + panelSpacing, topY,
	                                      rightWidth, panelHeight);

	// Position modulation controls in right panel
	int rightControlSize = 60;
	int rightControlX = rightPanel.getX() + (rightWidth - rightControlSize) / 2;
	int rightSliderX = rightPanel.getX() + 20;
	int rightSliderWidth = rightWidth - 40;
	
	lfoRateSlider->setBounds(rightControlX, rightPanel.getY() + 60, rightControlSize, rightControlSize);
	lfoDepthSlider->setBounds(rightSliderX, rightPanel.getY() + 130, rightSliderWidth, 20);
	envDepthSlider->setBounds(rightControlX, rightPanel.getY() + 180, rightControlSize, rightControlSize);
    mixSlider->setBounds(rightSliderX, rightPanel.getY() + 250, rightSliderWidth, 20);
    
    if (wetLabel)
        wetLabel->setBounds(rightPanel.getRight() - 35, rightPanel.getY() + 248, 30, 24);

    // Header controls - better positioning
    int headerY = 25;
    int headerSpacing = 70;
    int headerStartX = getWidth() - 300;
    
    if (crtToggle)
        crtToggle->setBounds(headerStartX, headerY, 60, 30);
    if (soloToggle)
        soloToggle->setBounds(headerStartX + headerSpacing, headerY, 60, 30);
    if (pairBox)
        pairBox->setBounds(headerStartX + 2 * headerSpacing, headerY, 100, 30);
}

void SimpleEditor::timerCallback()
{
	// Simulate spectrum data - in production this would come from DSP
	// Creating animated spectrum bars for visual effect
	static float phase = 0.0f;
	phase += 0.1f;

	for (int i = 0; i < 8; ++i)
	{
		// Generate pseudo-random spectrum levels with different frequencies per band
		float bandFreq = 0.5f + (i * 0.3f);
		float level = 0.3f + 0.4f * std::sin(phase * bandFreq) + 0.2f * std::sin(phase * bandFreq * 2.1f);
		level = juce::jlimit(0.0f, 1.0f, level);

		// Smooth the levels for realistic meter movement
		spectrumLevels[i] = spectrumLevels[i] * 0.85f + level * 0.15f;
	}

        // Update wet label from parameter
        float mixVal = 1.0f;
        if (auto* p = audioProcessor.getAPVTS().getRawParameterValue("mix")) mixVal = p->load();
        int pct = juce::jlimit(0, 100, (int)std::round(mixVal * 100.0f));
        if (wetLabel)
            wetLabel->setText(juce::String(pct) + "%", juce::dontSendNotification);

        // Repaint spectrum area for efficiency
        repaint(20, 470, 860, 160);
}

void SimpleEditor::drawOrbitDisplay(juce::Graphics& g, juce::Rectangle<int> bounds)
{
        // Minimal orbit display (no naming)
        static float galaxyPhase = 0.0f;
        galaxyPhase += 0.02f;

	// Dark space background
	g.setColour(juce::Colour(0xFF000814));
	g.fillRect(bounds);

        // Grid and unit circle
        g.setColour(juce::Colour(0x40404040));
        float centerX = bounds.getCentreX();
        float centerY = bounds.getCentreY();
        float radius = std::min(bounds.getWidth(), bounds.getHeight()) * 0.4f;

	// Unit circle
	g.drawEllipse(centerX - radius, centerY - radius, radius * 2, radius * 2, 1);

	// Grid lines
	g.drawLine(bounds.getX(), centerY, bounds.getRight(), centerY);
	g.drawLine(centerX, bounds.getY(), centerX, bounds.getBottom());

        // Draw 6 orbiting nodes
        const juce::Colour poleColors[6] = {
                juce::Colour(0xFF00D8FF),
                juce::Colour(0xFF00FFFF), // Cyan
                juce::Colour(0xFF55AAFF), // Light blue
                juce::Colour(0xFFFF00AA), // Magenta
                juce::Colour(0xFFFFFF55), // Yellow
                juce::Colour(0xFF55FF55)  // Green
        };

	for (int i = 0; i < 6; ++i)
	{
                // Simulate node positions with movement
                float baseAngle = (i * juce::MathConstants<float>::twoPi / 6.0f);
                float morphedAngle = baseAngle + 0.1f * std::sin(galaxyPhase + i * 0.5f);
                float morphedRadius = 0.7f + 0.2f * std::sin(galaxyPhase * 1.3f + i * 0.7f);
                morphedRadius = juce::jlimit(0.3f, 0.95f, morphedRadius);

		// Convert polar to cartesian
		float poleX = centerX + morphedRadius * radius * std::cos(morphedAngle);
		float poleY = centerY + morphedRadius * radius * std::sin(morphedAngle);

		// Draw pole with glow effect
		g.setColour(poleColors[i].withAlpha(0.3f));
		g.fillEllipse(poleX - 8, poleY - 8, 16, 16);
		g.setColour(poleColors[i]);
		g.fillEllipse(poleX - 4, poleY - 4, 8, 8);

		// Draw conjugate pole (mirror across x-axis)
		float conjPoleY = centerY - morphedRadius * radius * std::sin(morphedAngle);
		g.setColour(poleColors[i].withAlpha(0.3f));
		g.fillEllipse(poleX - 8, conjPoleY - 8, 16, 16);
		g.setColour(poleColors[i]);
		g.fillEllipse(poleX - 4, conjPoleY - 4, 8, 8);

		// Connect poles with morphing trajectories
		g.setColour(poleColors[i].withAlpha(0.2f));
		g.drawLine(poleX, poleY, poleX, conjPoleY, 1);
	}

        // Central morphing indicator
        float morphValue = 0.5f + 0.3f * std::sin(galaxyPhase * 2.0f);
        g.setColour(juce::Colour(0xFFFFFFAA).withAlpha(morphValue));
        g.fillEllipse(centerX - 6, centerY - 6, 12, 12);
        // Minimal: no readouts
}
