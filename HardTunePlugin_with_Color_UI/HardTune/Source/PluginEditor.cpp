
#include "PluginEditor.h"
#include "PluginProcessor.h"

using juce::Justification;

HardTuneAudioProcessorEditor::HardTuneAudioProcessorEditor (HardTuneAudioProcessor& p)
: juce::AudioProcessorEditor (&p), processor (p)
{
    setSize (560, 260);

    auto attachCombo = [&] (juce::ComboBox& box, const juce::String& paramID, std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>& att){
        addAndMakeVisible(box);
        att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts, paramID, box);
        return &box;
    };
    auto attachSlider = [&] (juce::Slider& s, const juce::String& paramID, std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& att){
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
        addAndMakeVisible(s);
        att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, paramID, s);
        return &s;
    };

    attachCombo(modeBox,   "mode", modeA);
    attachCombo(keyBox,    "key",  keyA);
    attachCombo(scaleBox,  "scale",scaleA);
    attachCombo(biasBox,   "bias", biasA);
    attachCombo(inputTypeBox, "inputType", inputA);

    attachSlider(retune, "retune", retuneA);
    attachSlider(amount, "amount", amountA);
    attachSlider(color,  "color",  colorA);
    attachSlider(formant,"formant",formantA);
    attachSlider(throat, "throat", throatA);
    attachSlider(mix,    "mix", mixA);
}

void HardTuneAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Premium dark gradient background
    auto bounds = getLocalBounds().toFloat();
    juce::Colour c1 = juce::Colour::fromFloatRGBA(0.06f, 0.07f, 0.10f, 1.0f);
    juce::Colour c2 = juce::Colour::fromFloatRGBA(0.02f, 0.02f, 0.03f, 1.0f);
    g.setGradientFill(juce::ColourGradient(c1, 0, 0, c2, 0, bounds.getHeight(), false));
    g.fillAll();

    // Header strip
    auto header = getLocalBounds().removeFromTop(36);
    juce::Colour accent = juce::Colours::orange.withBrightness(0.9f);
    g.setColour(accent.withAlpha(0.15f));
    g.fillRect(header.toFloat());

    // Title
    g.setColour(juce::Colours::white.withAlpha(0.9f));
    g.setFont(juce::Font(20.0f, juce::Font::bold));
    g.drawText("HardTune", header, juce::Justification::centredLeft);

    // Subtle border
    g.setColour(juce::Colours::white.withAlpha(0.06f));
    g.drawRoundedRectangle(bounds.reduced(1.0f), 8.0f, 1.0f);
}

void HardTuneAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(12);
    auto top = area.removeFromTop(36);
    modeBox.setBounds(top.removeFromLeft(100));
    keyBox.setBounds (top.removeFromLeft(80));
    scaleBox.setBounds (top.removeFromLeft(120));
    biasBox.setBounds  (top.removeFromLeft(100));
    inputTypeBox.setBounds(top);

    auto row = area.removeFromTop(170);
    auto w = row.getWidth() / 6;
    retune .setBounds(row.removeFromLeft(w).reduced(8));
    amount .setBounds(row.removeFromLeft(w).reduced(8));
    color  .setBounds(row.removeFromLeft(w).reduced(8));
    formant.setBounds(row.removeFromLeft(w).reduced(8));
    throat .setBounds(row.removeFromLeft(w).reduced(8));
    mix    .setBounds(row.removeFromLeft(w).reduced(8));
}
