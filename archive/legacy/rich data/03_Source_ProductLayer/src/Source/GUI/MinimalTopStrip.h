#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Core/Params.h"

class MinimalTopStrip : public juce::Component
{
public:
	explicit MinimalTopStrip(juce::AudioProcessorValueTreeState& state)
		: apvts(state)
	{
		// Drive slider (0..10)
		driveSlider.setRange(0.0, 10.0, 0.01);
		driveSlider.setTextValueSuffix(" d");
		driveSlider.setSliderStyle(juce::Slider::SliderStyle::LinearHorizontal);
		driveSlider.setTextBoxStyle(juce::Slider::TextEntryBoxPosition::TextBoxRight, false, 50, 18);
		addAndMakeVisible(driveSlider);
		driveLabel.setText("Drive", juce::dontSendNotification);
		driveLabel.attachToComponent(&driveSlider, true);
		addAndMakeVisible(driveLabel);
		driveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
			apvts, Params::ParameterIDs::saturationDrive, driveSlider);

		// Mix slider (0..1)
		mixSlider.setRange(0.0, 1.0, 0.001);
		mixSlider.setTextValueSuffix(" m");
		mixSlider.setSliderStyle(juce::Slider::SliderStyle::LinearHorizontal);
		mixSlider.setTextBoxStyle(juce::Slider::TextEntryBoxPosition::TextBoxRight, false, 50, 18);
		addAndMakeVisible(mixSlider);
		mixLabel.setText("Mix", juce::dontSendNotification);
		mixLabel.attachToComponent(&mixSlider, true);
		addAndMakeVisible(mixLabel);
		mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
			apvts, Params::ParameterIDs::saturationMix, mixSlider);
	}

	void paint(juce::Graphics& g) override
	{
		g.fillAll(juce::Colour(0xff111318));
	}

	void resized() override
	{
		auto r = getLocalBounds().reduced(6, 6);
		const int labelW = 48;
		const int spacing = 8;
		auto row = r;
		// Drive
		{
			auto driveArea = row.removeFromLeft(row.getWidth() / 2 - spacing / 2);
			driveSlider.setBounds(driveArea.removeFromLeft(driveArea.getWidth()).reduced(labelW, 0));
		}
		// Mix
		{
			auto mixArea = row;
			mixSlider.setBounds(mixArea.removeFromLeft(mixArea.getWidth()).reduced(labelW, 0));
		}
	}

private:
	juce::AudioProcessorValueTreeState& apvts;
	juce::Slider driveSlider, mixSlider;
	juce::Label driveLabel, mixLabel;
	std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> driveAttachment;
	std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;
};


