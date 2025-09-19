#include "PluginEditor.h"
#include "PluginProcessor.h"

PitchEngineEditor::PitchEngineEditor (PitchEngineAudioProcessor& p)
: juce::AudioProcessorEditor (&p), proc (p)
{
    setResizable (true, true);
    setSize (720, 420);

    auto addKnob = [this](juce::Slider& s, const juce::String& name)
    {
        addAndMakeVisible (s);
        s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 18);
        s.setName (name);
    };

    // Menus
    addAndMakeVisible (keyBox); keyBox.addItemList (juce::StringArray{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"}, 1);
    addAndMakeVisible (scaleBox); scaleBox.addItemList (juce::StringArray{"Chromatic","Major","Minor"}, 1);
    addAndMakeVisible (stabilizerBox); stabilizerBox.addItemList (juce::StringArray{"Off","Short","Mid","Long"}, 1);
    addAndMakeVisible (qualityBox); qualityBox.addItemList (juce::StringArray{"Track","Print"}, 1);

    // Knobs
    addKnob (retune,   "Retune");
    addKnob (strength, "Strength");
    addKnob (formant,  "Formant");
    addKnob (style,    "Style");

    // Buttons
    addAndMakeVisible (autoGainBtn);
    addAndMakeVisible (bypassBtn);
    addAndMakeVisible (secretBtn);

    auto& v = proc.apvts;
    aKey  = std::make_unique<A::ComboBoxAttachment> (v, "key", keyBox);
    aScale= std::make_unique<A::ComboBoxAttachment> (v, "scale", scaleBox);
    aStab = std::make_unique<A::ComboBoxAttachment> (v, "stabilizer", stabilizerBox);
    aQual = std::make_unique<A::ComboBoxAttachment> (v, "qualityMode", qualityBox);

    aRet  = std::make_unique<A::SliderAttachment>   (v, "retuneMs", retune);
    aStr  = std::make_unique<A::SliderAttachment>   (v, "strength", strength);
    aFrm  = std::make_unique<A::SliderAttachment>   (v, "formant",  formant);
    aSty  = std::make_unique<A::SliderAttachment>   (v, "style",    style);

    aAutoG= std::make_unique<A::ButtonAttachment>   (v, "autoGain", autoGainBtn);
    aByp  = std::make_unique<A::ButtonAttachment>   (v, "bypass",   bypassBtn);
    aSecret=std::make_unique<A::ButtonAttachment>   (v, "secretMode", secretBtn);
}

void PitchEngineEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xFF0B0F14));
    g.setColour (juce::Colours::white);
    g.setFont (18.0f);
    g.drawFittedText ("pitchEngine Pro — Live/Studio", getLocalBounds().removeFromTop (28), juce::Justification::centred, 1);
}

void PitchEngineEditor::resized()
{
    auto r = getLocalBounds().reduced (12);
    auto top = r.removeFromTop (40);
    keyBox.setBounds (top.removeFromLeft (120).reduced (4));
    scaleBox.setBounds (top.removeFromLeft (140).reduced (4));
    stabilizerBox.setBounds (top.removeFromLeft (120).reduced (4));
    qualityBox.setBounds (top.removeFromLeft (120).reduced (4));
    autoGainBtn.setBounds (top.removeFromLeft (100));
    bypassBtn.setBounds (top.removeFromLeft (80));
    secretBtn.setBounds (top.removeFromLeft (90));

    auto row = r.removeFromTop (200);
    auto col = [&row](int w){ auto x = row.removeFromLeft (w); return x.reduced (8); };

    retune  .setBounds (col (160));
    strength.setBounds (col (160));
    formant .setBounds (col (160));
    style   .setBounds (col (160));
}