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
    juce::TextButton hardTuneButton { "HARD-TUNE" };

    // Controls
    juce::ComboBox keyBox, scaleBox, stabilizerBox, qualityBox;
    juce::ToggleButton autoGainBtn { "AutoGain" }, bypassBtn { "Bypass" }, secretBtn { "Secret" };
    juce::Slider retune, strength, formant, style;

    using A = juce::AudioProcessorValueTreeState;
    std::unique_ptr<A::ComboBoxAttachment> aKey, aScale, aStab, aQual;
    std::unique_ptr<A::SliderAttachment> aRet, aStr, aFrm, aSty;
    std::unique_ptr<A::ButtonAttachment> aAutoG, aByp, aSecret;

    // Simple A/B UI flags (visual only)
    bool stateA = true, stateB = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchEngineEditor)
};