#include "FieldEngineFXEditor.h"

FieldEngineFXEditor::FieldEngineFXEditor (FieldEngineFXProcessor& p)
: juce::AudioProcessorEditor (&p), processor (p), valueTreeState(p.getAPVTS())
{
    setOpaque(true);
    setWantsKeyboardFocus(true);
    addAndMakeVisible(visualizer);
    
    auto setupSlider = [this](juce::Slider& slider, juce::Label& label, const juce::String& text)
    {
        addAndMakeVisible(slider);
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        
        addAndMakeVisible(label);
        label.setText(text, juce::dontSendNotification);
        label.attachToComponent(&slider, false);
        label.setJustificationType(juce::Justification::centred);
    };

    setupSlider(morphSlider, morphLabel, "Morph");
    setupSlider(intensitySlider, intensityLabel, "Intensity");
    setupSlider(driveSlider, driveLabel, "Drive");
    setupSlider(mixSlider, mixLabel, "Mix");
    setupSlider(lfoRateSlider, lfoRateLabel, "LFO Rate");
    setupSlider(lfoAmountSlider, lfoAmountLabel, "LFO Amount");
    
    morphAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(valueTreeState, "MORPH", morphSlider);
    intensityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(valueTreeState, "INTENSITY", intensitySlider);
    driveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(valueTreeState, "DRIVE", driveSlider);
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(valueTreeState, "mix", mixSlider);
    lfoRateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(valueTreeState, "LFO_RATE", lfoRateSlider);
    lfoAmountAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(valueTreeState, "LFO_AMOUNT", lfoAmountSlider);

    setSize(700, 520);
    startTimerHz(60);
}

FieldEngineFXEditor::~FieldEngineFXEditor()
{
    stopTimer();
}

void FieldEngineFXEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    g.fillAll(juce::Colour(0xFF0C0C0C));

    g.setColour(juce::Colour(0xFF00FF00));
    g.setFont(juce::FontOptions("Courier New", 16.0f, juce::Font::plain));
    auto header = bounds.removeFromTop(30);
    g.drawText("fieldEngine — anything = music", header, juce::Justification::centred);

    auto footer = bounds.removeFromBottom(25);
    g.setColour(juce::Colour(0xFF00FFFF));
    g.setFont(juce::FontOptions("Courier New", 12.0f, juce::Font::plain));
    g.drawText("V: mode  |  Wheel: LFO (+Shift=Amt)  |  Drag circle: morph", footer, juce::Justification::centred);
}

void FieldEngineFXEditor::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop(30);
    area.removeFromBottom(25);
    area = area.reduced(10);
    
    auto controlsArea = area.removeFromBottom(80);
    int sliderWidth = controlsArea.getWidth() / 6;
    
    morphSlider.setBounds(controlsArea.removeFromLeft(sliderWidth).reduced(5));
    intensitySlider.setBounds(controlsArea.removeFromLeft(sliderWidth).reduced(5));
    driveSlider.setBounds(controlsArea.removeFromLeft(sliderWidth).reduced(5));
    mixSlider.setBounds(controlsArea.removeFromLeft(sliderWidth).reduced(5));
    lfoRateSlider.setBounds(controlsArea.removeFromLeft(sliderWidth).reduced(5));
    lfoAmountSlider.setBounds(controlsArea.removeFromLeft(sliderWidth).reduced(5));
    
    visualizer.setBounds(area);
}

void FieldEngineFXEditor::timerCallback()
{
    auto& apvts = processor.getAPVTS();
    auto getParam = [&apvts](const juce::String& id, float defVal){ if (auto* p = apvts.getRawParameterValue(id)) return p->load(); return defVal; };

    float morph = getParam("MORPH", 0.5f);
    float lfoRate = getParam("LFO_RATE", 1.0f);
    float lfoAmt = getParam("LFO_AMOUNT", 0.1f);

    // Compute envelope from band energies (lock-free atomics)
    float env = 0.0f;
    const int n = processor.getNumBands();
    for (int i = 0; i < n; ++i) env += processor.getBandEnergy(i);
    env = (n > 0) ? juce::jlimit(0.0f, 1.0f, env / (float)n) : 0.0f;

    std::array<float, 32> resp{};
    for (size_t i = 0; i < resp.size(); ++i)
        resp[i] = 0.5f + 0.5f * std::sin((float)i * 0.2f + morph * juce::MathConstants<float>::twoPi);

    visualizer.updateFilterResponse(resp);
    visualizer.updateMorphPosition(morph);
    visualizer.updateLFOValue(std::sin(juce::Time::getMillisecondCounterHiRes() * 0.001 * lfoRate) * lfoAmt);
    visualizer.updateEnvelope(env);
    visualizer.timerCallback();
}

bool FieldEngineFXEditor::keyPressed (const juce::KeyPress& key)
{
    if (key.getTextCharacter() == 'v' || key.getTextCharacter() == 'V')
    {
        visualizer.cycleMode();
        repaint();
        return true;
    }
    return false;
}
