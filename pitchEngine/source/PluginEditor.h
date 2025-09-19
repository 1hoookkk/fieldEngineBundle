#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
class PitchEngineAudioProcessor;

class PitchEngineEditor : public juce::AudioProcessorEditor
{
public:
    explicit PitchEngineEditor (PitchEngineAudioProcessor&);
    ~PitchEngineEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    PitchEngineAudioProcessor& proc;

    // Controls
    juce::ComboBox keyBox, scaleBox, stabilizerBox, qualityBox;
    juce::ToggleButton autoGainBtn { "AutoGain" }, bypassBtn { "Bypass" }, secretBtn { "Secret" };
    juce::Slider retune, strength, formant, style;

    using A = juce::AudioProcessorValueTreeState;
    std::unique_ptr<A::ComboBoxAttachment> aKey, aScale, aStab, aQual;
    std::unique_ptr<A::SliderAttachment> aRet, aStr, aFrm, aSty;
    std::unique_ptr<A::ButtonAttachment> aAutoG, aByp, aSecret;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchEngineEditor)
};