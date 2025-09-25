#include <algorithm>
#include "TerminalMorphUI.h"

//==============================================================================
// Modern dark theme color definitions
const juce::Colour TerminalMorphUI::terminalBlue(0xFF00d4ff);      // Cyan accent
const juce::Colour TerminalMorphUI::terminalBlack(0xFF1a1a1a);     // Dark background
const juce::Colour TerminalMorphUI::terminalGreen(0xFF00d4ff);     // Cyan for highlights
const juce::Colour TerminalMorphUI::terminalYellow(0xFF00d4ff);    // Cyan for active elements
const juce::Colour TerminalMorphUI::terminalRed(0xFFff4444);       // Subtle red for warnings
const juce::Colour TerminalMorphUI::terminalWhite(0xFFe6e6e6);     // Soft white text

//==============================================================================
TerminalMorphUI::TerminalMorphUI(MorphEngineAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize (500, 200);  // Modern studio utility size

    // Main amount/intensity control (replaces mix slider)
    amountSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    amountSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    amountSlider.setRange (0.0, 1.0, 0.001);
    amountSlider.setColour (juce::Slider::trackColourId, terminalBlack.brighter(0.1f));
    amountSlider.setColour (juce::Slider::thumbColourId, terminalBlue);
    addAndMakeVisible (amountSlider);

    amountLabel.setText ("AMOUNT", juce::dontSendNotification);
    amountLabel.setJustificationType (juce::Justification::centredLeft);
    amountLabel.setColour (juce::Label::textColourId, terminalWhite);
    amountLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    addAndMakeVisible (amountLabel);

    // Authentic EMU preset browser - using real extracted presets
    // Show first 6 EMU AIR presets for clean minimal interface
    for (int i = 0; i < 6; ++i)
    {
        auto& button = presetButtons[i];
        // Extract short names from the real EMU presets (processor.getPresetName())
        juce::String presetName = processor.getPresetName(i);
        if (presetName.contains(":"))
            presetName = presetName.fromFirstOccurrenceOf(":", false, true); // Get part after ":"

        button.setButtonText (presetName.substring(0, 8)); // Truncate for clean layout
        button.setColour (juce::TextButton::buttonColourId, terminalBlack.brighter(0.05f));
        button.setColour (juce::TextButton::buttonOnColourId, terminalBlue);
        button.setColour (juce::TextButton::textColourOnId, terminalBlack);
        button.setColour (juce::TextButton::textColourOffId, terminalWhite);
        button.onClick = [this, i]() { handlePresetButton (i); };
        addAndMakeVisible (button);
    }

    // Connect amount slider to main mix parameter
    amountAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, "style.mix", amountSlider);

    processor.apvts.addParameterListener ("style.mix", this);

    // Initialize frequency response visualization timer
    startTimerHz(24); // 24fps for smooth response curve updates

    currentAmount = processor.apvts.getRawParameterValue ("style.mix")->load();
}

TerminalMorphUI::~TerminalMorphUI()
{
    stopTimer();
    processor.apvts.removeParameterListener("style.mix", this);
}

void TerminalMorphUI::paint(juce::Graphics& g)
{
    // Modern dark background
    g.fillAll (terminalBlack);

    // Draw minimal title bar
    drawTitleBar(g, titleArea);

    // Draw compact frequency response visualization
    drawFrequencyResponse(g, visualizerArea);
}

void TerminalMorphUI::resized()
{
    auto bounds = getLocalBounds();
    const int padding = 8;

    // Title area (compact)
    titleArea = bounds.removeFromTop (24);

    // Frequency response visualization (compact but prominent)
    visualizerArea = bounds.removeFromTop (80).reduced (padding, padding / 2);

    // Amount control area
    sliderArea = bounds.removeFromTop (40);
    amountLabel.setBounds (sliderArea.removeFromLeft (60).reduced (padding, 0));
    amountSlider.setBounds (sliderArea.reduced (padding, 8));

    // Preset browser (remaining space)
    presetArea = bounds.reduced (padding, padding / 2);
    auto buttonWidth = presetArea.getWidth() / 6;
    for (int i = 0; i < 6; ++i)
    {
        auto buttonBounds = presetArea.removeFromLeft (buttonWidth).reduced (2, 0);
        presetButtons[i].setBounds (buttonBounds);
    }
}

void TerminalMorphUI::timerCallback()
{
    updateFrequencyResponse();
    repaint (visualizerArea); // Only repaint visualizer for efficiency
}

// Mouse interaction removed - minimal utility interface

void TerminalMorphUI::drawTitleBar(juce::Graphics& g, const juce::Rectangle<int>& bounds)
{
    // Minimal dark title bar
    g.setColour(terminalBlack.brighter(0.02f));
    g.fillRect(bounds);

    g.setColour(terminalWhite);
    g.setFont(juce::Font(juce::FontOptions(11.0f)));

    // Clean title with current preset info
    juce::String currentPreset = processor.getPresetName(selectedPreset);
    if (currentPreset.contains(":"))
        currentPreset = currentPreset.fromFirstOccurrenceOf(":", false, true);

    juce::String titleText = "morphEngine • " + currentPreset;
    g.drawText(titleText, bounds.reduced(8, 2), juce::Justification::centredLeft);
}

void TerminalMorphUI::drawFrequencyResponse(juce::Graphics& g, const juce::Rectangle<int>& bounds)
{
    // Clean dark background
    g.setColour(terminalBlack.brighter(0.01f));
    g.fillRect(bounds);

    // Subtle cyan border
    g.setColour(terminalBlue.withAlpha(0.3f));
    g.drawRect(bounds, 1);

    // Draw frequency response curve
    if (frequencyResponse.size() > 1)
    {
        juce::Path responseCurve;
        auto curveBounds = bounds.reduced(4).toFloat();

        // Start path
        float x = curveBounds.getX();
        float y = curveBounds.getBottom() - (frequencyResponse[0] * curveBounds.getHeight());
        responseCurve.startNewSubPath(x, y);

        // Draw smooth curve through frequency response points
        float xStep = curveBounds.getWidth() / (float)(frequencyResponse.size() - 1);
        for (size_t i = 1; i < frequencyResponse.size(); ++i)
        {
            x += xStep;
            y = curveBounds.getBottom() - (frequencyResponse[i] * curveBounds.getHeight());
            responseCurve.lineTo(x, y);
        }

        // Draw the curve with cyan accent
        g.setColour(terminalBlue.withAlpha(0.8f));
        g.strokePath(responseCurve, juce::PathStrokeType(1.5f));

        // Add subtle fill under curve
        responseCurve.lineTo(curveBounds.getRight(), curveBounds.getBottom());
        responseCurve.lineTo(curveBounds.getX(), curveBounds.getBottom());
        responseCurve.closeSubPath();

        g.setColour(terminalBlue.withAlpha(0.1f));
        g.fillPath(responseCurve);
    }

    // Add frequency labels (minimal)
    g.setColour(terminalWhite.withAlpha(0.4f));
    g.setFont(juce::Font(juce::FontOptions(9.0f)));
    auto labelBounds = bounds.withTrimmedTop(bounds.getHeight() - 12);
    g.drawText("20Hz", labelBounds.withWidth(30), juce::Justification::left);
    g.drawText("20kHz", labelBounds.withTrimmedLeft(labelBounds.getWidth() - 30), juce::Justification::right);
}

void TerminalMorphUI::updateFrequencyResponse()
{
    // Get current filter state from the processor for real-time visualization
    currentAmount = processor.apvts.getRawParameterValue("style.mix")->load();

    // Generate approximated frequency response based on current EMU filter state
    // This creates a visual representation of what the Z-plane filter is doing
    const float amount = currentAmount.load();

    for (size_t i = 0; i < frequencyResponse.size(); ++i)
    {
        // Logarithmic frequency mapping (20Hz to 20kHz)
        float logFreq = std::log10(20.0f + (20000.0f - 20.0f) * ((float)i / (float)(frequencyResponse.size() - 1)));
        float normalizedLogFreq = (logFreq - std::log10(20.0f)) / (std::log10(20000.0f) - std::log10(20.0f));

        // Create characteristic EMU Z-plane response curve
        // Higher amount creates more resonant peaks and filtering
        float response = 0.5f; // Baseline

        if (amount > 0.1f)
        {
            // Add resonant peaks typical of EMU filtering
            float peak1 = amount * 0.4f * std::sin(normalizedLogFreq * juce::MathConstants<float>::pi * 2.5f);
            float peak2 = amount * 0.3f * std::sin(normalizedLogFreq * juce::MathConstants<float>::pi * 4.7f);
            response += peak1 + peak2 * 0.7f;
        }

        // Smooth interpolation for organic curve
        response = juce::jlimit(0.0f, 1.0f, response);

        // Apply gentle smoothing to previous value for stable animation
        const float smoothing = 0.8f;
        frequencyResponse[i] = smoothing * frequencyResponse[i] + (1.0f - smoothing) * response;
    }
}

void TerminalMorphUI::handlePresetButton(int buttonIndex)
{
    // Load authentic EMU preset directly (first 6 AIR presets)
    processor.loadPreset(buttonIndex);
    selectedPreset = buttonIndex;

    // Update button states to show active preset
    for (int i = 0; i < 6; ++i)
    {
        presetButtons[i].setToggleState(i == buttonIndex, juce::dontSendNotification);
    }

    repaint(); // Update title bar and visualization
}


void TerminalMorphUI::parameterChanged(const juce::String& parameterID, float newValue)
{
    if (parameterID == "style.mix")
    {
        currentAmount = newValue;
        repaint(visualizerArea); // Update frequency response
    }
}
