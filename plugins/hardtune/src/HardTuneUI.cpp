#include "HardTuneUI.h"
#include "HardTuneProcessor.h"

#include <array>
#include <algorithm>

namespace
{
    constexpr int defaultWidth  = 720;
    constexpr int defaultHeight = 360;

    constexpr std::array<float, 3> macroRetune { 0.65f, 0.85f, 1.00f };
    constexpr std::array<float, 3> macroAmount { 0.70f, 0.90f, 1.00f };
    constexpr std::array<float, 3> macroMix    { 0.75f, 0.90f, 1.00f };
    constexpr std::array<float, 3> macroColor  { 0.18f, 0.20f, 0.25f };

    constexpr float retuneTolerance = 0.05f;
    constexpr float amountTolerance = 0.05f;
    constexpr float mixTolerance    = 0.05f;
    constexpr float colorTolerance  = 0.05f;
}

HardTuneUI::HardTuneUI(HardTuneProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(defaultWidth, defaultHeight);
    setResizable(false, false);

    configureCombo(modeBox,   "mode",    modeAttachment);
    configureCombo(keyBox,    "key",     keyAttachment);
    configureCombo(scaleBox,  "scale",   scaleAttachment);
    configureCombo(biasBox,   "bias",    biasAttachment);
    configureCombo(inputTypeBox, "inputType", inputTypeAttachment);

    configureSlider(retuneSlider,  retuneLabel,  "Retune",  "retune",  retuneAttachment);
    configureSlider(amountSlider,  amountLabel,  "Amount",  "amount",  amountAttachment);
    configureSlider(colorSlider,   colorLabel,   "Color",   "color",   colorAttachment);
    configureSlider(formantSlider, formantLabel, "Formant", "formant", formantAttachment);
    configureSlider(throatSlider,  throatLabel,  "Throat",  "throat",  throatAttachment);
    configureSlider(mixSlider,     mixLabel,     "Mix",     "mix",     mixAttachment);

    mixSlider.setTextValueSuffix(" %");
    mixSlider.textFromValueFunction = [] (double v) { return juce::String(juce::roundToInt(v * 100.0)) + "%"; };
    mixSlider.valueFromTextFunction = [] (const juce::String& text) {
        auto trimmed = text.upToFirstOccurrenceOf("%", false, false).trim();
        return juce::jlimit(0.0, 1.0, trimmed.getDoubleValue() / 100.0);
    };

    const int macroGroup = 0x6200;
    for (auto* btn : { &naturalBtn, &tightBtn, &hardBtn })
    {
        addAndMakeVisible(*btn);
        btn->setClickingTogglesState(true);
        btn->setRadioGroupId(macroGroup);
        btn->setColour(juce::TextButton::buttonColourId, juce::Colour::fromFloatRGBA(0.12f, 0.14f, 0.18f, 1.0f));
        btn->setColour(juce::TextButton::buttonOnColourId, juce::Colours::orange.withAlpha(0.35f));
    }
    naturalBtn.onClick = [this] { applyMacroStep(1); };
    tightBtn.onClick   = [this] { applyMacroStep(2); };
    hardBtn.onClick    = [this] { applyMacroStep(3); };

    addAndMakeVisible(categoryBox);
    categoryBox.addItemList(juce::StringArray { "ALL", "LIVE", "STUDIO", "CREATIVE" }, 1);
    categoryBox.setSelectedId(1, juce::dontSendNotification);
    categoryBox.onChange = [this]
    {
        currentCategory = categoryBox.getText();
        refreshPresetList();
    };

    addAndMakeVisible(presetBox);
    presetBox.onChange = [this]
    {
        const int sel = presetBox.getSelectedId() - 1;
        if (sel >= 0 && sel < (int) filteredPresetIndices.size())
            processor.loadPreset(filteredPresetIndices[(size_t) sel]);
    };

    addAndMakeVisible(prevPreset);
    prevPreset.onClick = [this]
    {
        int sel = presetBox.getSelectedId() - 1;
        if (filteredPresetIndices.empty())
            return;
        sel = (sel <= 0) ? (int) filteredPresetIndices.size() - 1 : sel - 1;
        presetBox.setSelectedId(sel + 1, juce::sendNotification);
    };

    addAndMakeVisible(nextPreset);
    nextPreset.onClick = [this]
    {
        int sel = presetBox.getSelectedId() - 1;
        if (filteredPresetIndices.empty())
            return;
        sel = (sel + 1) % (int) filteredPresetIndices.size();
        presetBox.setSelectedId(sel + 1, juce::sendNotification);
    };

    refreshPresetList();
    updateMacroButtons();

    startTimerHz(30);
}

HardTuneUI::~HardTuneUI()
{
    stopTimer();
}

void HardTuneUI::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    const auto topColour    = juce::Colour::fromFloatRGBA(0.06f, 0.07f, 0.10f, 1.0f);
    const auto bottomColour = juce::Colour::fromFloatRGBA(0.02f, 0.02f, 0.03f, 1.0f);

    g.setGradientFill(juce::ColourGradient(topColour, 0, 0, bottomColour, 0, bounds.getHeight(), false));
    g.fillAll();

    auto header = juce::Rectangle<int>(0, 0, getWidth(), 48);
    g.setColour(juce::Colours::orange.withAlpha(0.18f));
    g.fillRect(header);

    g.setColour(juce::Colours::white.withAlpha(0.9f));
    g.setFont(juce::Font(22.0f, juce::Font::bold));
    g.drawText("HardTune", header.reduced(16, 0), juce::Justification::centredLeft);

    g.setColour(juce::Colours::white.withAlpha(0.07f));
    g.drawRoundedRectangle(bounds.reduced(1.0f), 8.0f, 1.0f);
}

void HardTuneUI::resized()
{
    auto area = getLocalBounds().reduced(18);

    auto header = area.removeFromTop(48);
    auto titleArea = header.removeFromLeft(220);
    (void) titleArea;

    header.removeFromLeft(12);
    auto modeArea = header.removeFromLeft(100);
    modeBox.setBounds(modeArea);

    header.removeFromLeft(8);
    auto keyArea = header.removeFromLeft(80);
    keyBox.setBounds(keyArea);

    header.removeFromLeft(6);
    auto scaleArea = header.removeFromLeft(120);
    scaleBox.setBounds(scaleArea);

    header.removeFromLeft(6);
    auto biasArea = header.removeFromLeft(90);
    biasBox.setBounds(biasArea);

    header.removeFromLeft(6);
    auto inputArea = header.removeFromLeft(110);
    inputTypeBox.setBounds(inputArea);

    header.removeFromLeft(8);
    naturalBtn.setBounds(header.removeFromLeft(90));
    header.removeFromLeft(4);
    tightBtn.setBounds(header.removeFromLeft(80));
    header.removeFromLeft(4);
    hardBtn.setBounds(header.removeFromLeft(70));

    header.removeFromLeft(8);
    prevPreset.setBounds(header.removeFromLeft(24));
    header.removeFromLeft(4);
    nextPreset.setBounds(header.removeFromLeft(24));
    header.removeFromLeft(6);
    categoryBox.setBounds(header.removeFromLeft(90));
    header.removeFromLeft(6);
    presetBox.setBounds(header);

    area.removeFromTop(16);

    auto controls = area.removeFromTop(170);
    const int columnWidth = controls.getWidth() / 3;

    auto placeColumn = [&controls, columnWidth]() -> juce::Rectangle<int>
    {
        auto slot = controls.removeFromLeft(columnWidth);
        return slot.reduced(12, 0);
    };

    auto leftColumn = placeColumn();
    auto centerColumn = placeColumn();
    auto rightColumn = controls.reduced(12, 0);

    auto placeSlider = [](juce::Rectangle<int> column, juce::Slider& slider, juce::Label& label)
    {
        auto labelArea = column.removeFromTop(24);
        label.setBounds(labelArea);
        slider.setBounds(column.reduced(0, 12));
    };

    placeSlider(leftColumn.removeFromTop(140), retuneSlider, retuneLabel);
    placeSlider(leftColumn.removeFromTop(140), amountSlider, amountLabel);

    placeSlider(centerColumn.removeFromTop(140), mixSlider, mixLabel);
    placeSlider(centerColumn.removeFromTop(140), colorSlider, colorLabel);

    placeSlider(rightColumn.removeFromTop(140), formantSlider, formantLabel);
    placeSlider(rightColumn.removeFromTop(140), throatSlider, throatLabel);

    area.removeFromTop(12);
    // leave remainder for future visualiser/placeholder if desired
}

void HardTuneUI::timerCallback()
{
    updateMacroButtons();
    const auto current = processor.getCurrentPresetName();
    if (current != lastPresetName)
    {
        lastPresetName = current;
        refreshPresetList();
    }
}

void HardTuneUI::configureCombo(juce::ComboBox& box, const juce::String& paramID,
                                std::unique_ptr<ComboAttachment>& attachment)
{
    box.setJustificationType(juce::Justification::centred);
    box.setColour(juce::ComboBox::backgroundColourId, juce::Colour::fromFloatRGBA(0.18f, 0.18f, 0.20f, 1.0f));
    box.setColour(juce::ComboBox::textColourId, juce::Colours::whitesmoke);
    box.setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(box);
    attachment = std::make_unique<ComboAttachment>(processor.apvts, paramID, box);
}

void HardTuneUI::configureSlider(juce::Slider& slider, juce::Label& label,
                                 const juce::String& name, const juce::String& paramID,
                                 std::unique_ptr<SliderAttachment>& attachment)
{
    label.setText(name, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, juce::Colours::whitesmoke.withAlpha(0.9f));
    addAndMakeVisible(label);

    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 20);
    slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::orange.withAlpha(0.80f));
    slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colours::orange.withAlpha(0.2f));
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::whitesmoke);
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setPopupDisplayEnabled(true, false, this);
    addAndMakeVisible(slider);

    attachment = std::make_unique<SliderAttachment>(processor.apvts, paramID, slider);
}

void HardTuneUI::applyMacroStep(int step, bool withGesture)
{
    step = juce::jlimit(1, 3, step);
    setParamFloat("retune", macroRetune[(size_t) (step - 1)], withGesture);
    setParamFloat("amount", macroAmount[(size_t) (step - 1)], withGesture);
    setParamFloat("mix",    macroMix   [(size_t) (step - 1)], withGesture);
    setParamFloat("color",  macroColor [(size_t) (step - 1)], withGesture);
}

void HardTuneUI::setParamFloat(const juce::String& paramID, float value, bool withGesture)
{
    if (auto* param = processor.apvts.getParameter(paramID))
    {
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(param))
        {
            float norm = ranged->convertTo0to1(value);
            if (withGesture) param->beginChangeGesture();
            param->setValueNotifyingHost(norm);
            if (withGesture) param->endChangeGesture();
        }
    }
}

void HardTuneUI::updateMacroButtons()
{
    const int step = detectMacroStep();
    naturalBtn.setToggleState(step == 1, juce::dontSendNotification);
    tightBtn.setToggleState  (step == 2, juce::dontSendNotification);
    hardBtn.setToggleState   (step == 3, juce::dontSendNotification);
}

int HardTuneUI::detectMacroStep() const
{
    const float retune = processor.apvts.getRawParameterValue("retune")->load();
    const float amount = processor.apvts.getRawParameterValue("amount")->load();
    const float mix    = processor.apvts.getRawParameterValue("mix")->load();
    const float color  = processor.apvts.getRawParameterValue("color")->load();

    for (int i = 0; i < 3; ++i)
    {
        if (std::abs(retune - macroRetune[(size_t) i]) <= retuneTolerance &&
            std::abs(amount - macroAmount[(size_t) i]) <= amountTolerance &&
            std::abs(mix - macroMix[(size_t) i])       <= mixTolerance &&
            std::abs(color - macroColor[(size_t) i])   <= colorTolerance)
            return i + 1;
    }
    return 0;
}

void HardTuneUI::refreshPresetList()
{
    presetBox.clear(juce::dontSendNotification);
    filteredPresetIndices.clear();

    const int total = processor.getNumPresets();
    for (int i = 0; i < total; ++i)
    {
        auto name = processor.getPresetName(i);
        if (currentCategory != "ALL")
        {
            if (! name.startsWithIgnoreCase(currentCategory + ":"))
                continue;
        }
        filteredPresetIndices.push_back(i);
    }

    int id = 1;
    int selectedId = 0;
    const auto current = processor.getCurrentPresetName();
    for (int idx : filteredPresetIndices)
    {
        auto nm = processor.getPresetName(idx);
        presetBox.addItem(nm, id);
        if (nm == current)
            selectedId = id;
        ++id;
    }

    if (selectedId == 0 && ! filteredPresetIndices.empty())
        selectedId = 1;

    if (selectedId > 0)
        presetBox.setSelectedId(selectedId, juce::dontSendNotification);
}


