#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include "ui/LookAndFeelPE.h"
#include "ui/HeaderBar.h"
#include "ui/MeterMini.h"

class PitchEngineAudioProcessor;

class PitchEngineEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PitchEngineEditor (PitchEngineAudioProcessor&);
    ~PitchEngineEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    PitchEngineAudioProcessor& proc;
    LookAndFeelPE lnf;

    // Header + meters
    HeaderBar header;
    MeterMini meters;

    // Controls
    juce::ComboBox keyBox, scaleBox, stabilizerBox, qualityBox, classicFilterBox;
    juce::ToggleButton autoGainBtn { "AutoGain" }, bypassBtn { "Bypass" }, secretBtn { "Secret" }, classicModeBtn { "Classic" };
    juce::Slider retune, strength, formant, style;

    using A = juce::AudioProcessorValueTreeState;
    std::unique_ptr<A::ComboBoxAttachment> aKey, aScale, aStab, aQual, aClassicFilter;
    std::unique_ptr<A::SliderAttachment> aRet, aStr, aFrm, aSty;
    std::unique_ptr<A::ButtonAttachment> aAutoG, aByp, aSecret, aClassicMode;

    // Simple A/B UI flags (visual only)
    bool stateA = true, stateB = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchEngineEditor)
};