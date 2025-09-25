#pragma once

#include "JuceHeader.h"
#include "../fx/FieldEngineFXProcessor.h"
#include "../shared/AsciiVisualizer.h"

class SimpleEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
	SimpleEditor(FieldEngineFXProcessor&);
	~SimpleEditor() override;

	void paint (juce::Graphics& g) override;
	void resized() override;

private:
	FieldEngineFXProcessor& audioProcessor;

	// UI Components
	std::unique_ptr<AsciiVisualizer> visualizer;

	// MetaSynth/Temple style controls
	std::unique_ptr<juce::Slider> morphSlider;
	std::unique_ptr<juce::Slider> intensitySlider;
	std::unique_ptr<juce::Slider> driveSlider;
	std::unique_ptr<juce::Slider> lfoRateSlider;
	std::unique_ptr<juce::Slider> lfoDepthSlider;
	std::unique_ptr<juce::Slider> envDepthSlider;
	std::unique_ptr<juce::Slider> mixSlider;

	// Minimal pair selector (vowel/bell/low)
	std::unique_ptr<juce::ComboBox> pairBox;
	std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> pairAttachment;

	// CRT overlay toggle
	std::unique_ptr<juce::ToggleButton> crtToggle;
	std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> crtAttachment;

	// Solo effect toggle
	std::unique_ptr<juce::ToggleButton> soloToggle;
	std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> soloAttachment;

	// Tiny wet % readout next to mix
	std::unique_ptr<juce::Label> wetLabel;

	// 8-band spectrum meters
	std::array<float, 8> spectrumLevels;

	void setupControls();
	void timerCallback() override;
        void drawOrbitDisplay(juce::Graphics& g, juce::Rectangle<int> bounds);
        void drawModernPanel(juce::Graphics& g, juce::Rectangle<int> bounds, 
                            const juce::String& title, juce::Colour accentColor);
        void drawModernSpectrum(juce::Graphics& g, juce::Rectangle<int> bounds);

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SimpleEditor)
};
