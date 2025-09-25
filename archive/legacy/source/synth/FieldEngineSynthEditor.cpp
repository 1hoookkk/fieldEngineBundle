#include "FieldEngineSynthEditor.h"

FieldEngineSynthEditor::FieldEngineSynthEditor (FieldEngineSynthProcessor& p)
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

    setupSlider(detuneSlider, detuneLabel, "Detune");
    setupSlider(cutoffSlider, cutoffLabel, "Cutoff");
    setupSlider(resonanceSlider, resonanceLabel, "Resonance");
    setupSlider(attackSlider, attackLabel, "Attack");
    setupSlider(decaySlider, decayLabel, "Decay");
    setupSlider(sustainSlider, sustainLabel, "Sustain");
    setupSlider(releaseSlider, releaseLabel, "Release");
    
    detuneAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(valueTreeState, "DETUNE", detuneSlider);
    cutoffAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(valueTreeState, "CUTOFF", cutoffSlider);
    resonanceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(valueTreeState, "RESONANCE", resonanceSlider);
    attackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(valueTreeState, "ATTACK", attackSlider);
    decayAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(valueTreeState, "DECAY", decaySlider);
    sustainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(valueTreeState, "SUSTAIN", sustainSlider);
    releaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(valueTreeState, "RELEASE", releaseSlider);
    
    setSize(700, 520);
    startTimerHz(30);
}

FieldEngineSynthEditor::~FieldEngineSynthEditor()
{
    stopTimer();
}

void FieldEngineSynthEditor::paint (juce::Graphics& g)
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
    g.drawText("V: Visual Mode  |  M: Morph Reset  |  R: Reset  |  8 VOICES", footer, juce::Justification::centred);
}

void FieldEngineSynthEditor::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop(30);
    area.removeFromBottom(25);
    area = area.reduced(10);
    
    auto controlsArea = area.removeFromBottom(160);
    auto filterArea = controlsArea.removeFromTop(80);
    auto envArea = controlsArea;
    
    int filterSliderWidth = filterArea.getWidth() / 3;
    detuneSlider.setBounds(filterArea.removeFromLeft(filterSliderWidth).reduced(5));
    cutoffSlider.setBounds(filterArea.removeFromLeft(filterSliderWidth).reduced(5));
    resonanceSlider.setBounds(filterArea.removeFromLeft(filterSliderWidth).reduced(5));
    
    int envSliderWidth = envArea.getWidth() / 4;
    attackSlider.setBounds(envArea.removeFromLeft(envSliderWidth).reduced(5));
    decaySlider.setBounds(envArea.removeFromLeft(envSliderWidth).reduced(5));
    sustainSlider.setBounds(envArea.removeFromLeft(envSliderWidth).reduced(5));
    releaseSlider.setBounds(envArea.removeFromLeft(envSliderWidth).reduced(5));
    
    visualizer.setBounds(area);
}

void FieldEngineSynthEditor::timerCallback()
{
    auto& apvts = processor.getAPVTS();
    auto getParam = [&apvts](const juce::String& id, float defVal){ if (auto* p = apvts.getRawParameterValue(id)) return p->load(); return defVal; };

    float morph = getParam("MORPH", 0.5f);
    float cutoff = getParam("CUTOFF", 1000.0f);
    float resonance = getParam("RESONANCE", 1.0f);
    float env = 0.0f; // processor maintains its own simple follower

    // Not accessible directly; we keep it simple for UI
    // Optionally, expose a getter to read envelopeFollower

    // Compose a simple response curve for the UI
    std::array<float, 32> resp{};
    for (size_t i = 0; i < resp.size(); ++i)
    {
        float x = (float)i / (resp.size() - 1);
        float f = 20.0f * std::pow(1000.0f, x);
        float nf = f / juce::jmax(20.0f, cutoff);
        float mag = nf > 1.0f ? 1.0f / (nf * nf) : 1.0f;
        mag *= (1.0f + 0.1f * (resonance - 1.0f));
        resp[i] = juce::jlimit(0.0f, 1.0f, mag);
    }

    visualizer.updateFilterResponse(resp);
    visualizer.updateMorphPosition(morph);
    visualizer.updateEnvelope(env);
    visualizer.updateLFOValue(0.0f);
    repaint();
}
