#include "PluginEditor.h"
#include "PluginProcessor.h"

PitchEngineEditor::PitchEngineEditor (PitchEngineAudioProcessor& p)
: juce::AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    setResizable (true, true);
    setSize (780, 440);

    // Header callbacks (wire to params)
    header.onAToggle      = [this]{ stateA = true;  stateB = false; repaint(); };
    header.onBToggle      = [this]{ stateA = false; stateB = true;  repaint(); };
    header.onBypassToggle = [this]{ bypassBtn.setToggleState (!bypassBtn.getToggleState(), juce::dontSendNotification);
                                    bypassBtn.triggerClick(); };
    header.onQualityToggle= [this]{
        auto v = qualityBox.getSelectedItemIndex();
        qualityBox.setSelectedItemIndex (v == 0 ? 1 : 0);
    };
    header.onSecretToggle = [this]{ secretBtn.setToggleState (!secretBtn.getToggleState(), juce::dontSendNotification);
                                    secretBtn.triggerClick(); };

    addAndMakeVisible (header);

    auto addKnob = [this](juce::Slider& s, const juce::String& name, const juce::String& tip)
    {
        addAndMakeVisible (s);
        s.setName (name);
        s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 62, 18);
        s.setTooltip (tip);
        s.setDoubleClickReturnValue (true, s.getDoubleClickReturnValue(), true);
    };
    auto addBox = [this](juce::ComboBox& b, const juce::String& tip){ addAndMakeVisible (b); b.setTooltip (tip); };
    auto addBtn = [this](juce::Button& b,   const juce::String& tip){ addAndMakeVisible (b); b.setTooltip (tip); };

    // Menus
    addBox (keyBox, "Root key");
    keyBox.addItemList (juce::StringArray { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" }, 1);
    addBox (scaleBox, "Scale");
    scaleBox.addItemList (juce::StringArray { "Chromatic","Major","Minor" }, 1);
    addBox (stabilizerBox, "Hold time to reduce hunting");
    stabilizerBox.addItemList (juce::StringArray { "Off","Short","Mid","Long" }, 1);
    addBox (qualityBox, "Track: lowest latency. Print: best quality (PDC).");
    qualityBox.addItemList (juce::StringArray { "Track","Print" }, 1);
    addBox (classicFilterBox, "Classic Mode EMU filter style: Velvet (smooth), Air (bright), Focus (clear)");
    classicFilterBox.addItemList (juce::StringArray { "Velvet","Air","Focus" }, 1);

    // Knobs
    addKnob (retune,   "Retune",   "Lower = faster snap (1–20 ms hard, 50–200 ms natural).");
    addKnob (strength, "Strength", "How strongly to hold the target note; lower keeps vibrato.");
    addKnob (formant,  "Formant",  "Preserve vocal timbre during large shifts.");
    addKnob (style,    "Style",    "Adds depth/focus; live-safe.");

    // Buttons
    addBtn (autoGainBtn, "Matches output to bypassed level (±0.5 dB).");
    addBtn (bypassBtn,   "Click-safe 10 ms crossfade bypass.");
    addBtn (secretBtn,   "Alternate path with refined tone and motion.");
    addBtn (classicModeBtn, "Classic Mode: Auto-Tune 5 style hard snap with EMU filter character.");

    // Attach
    auto& v = proc.apvts;
    aKey   = std::make_unique<A::ComboBoxAttachment> (v, "key",         keyBox);
    aScale = std::make_unique<A::ComboBoxAttachment> (v, "scale",       scaleBox);
    aStab  = std::make_unique<A::ComboBoxAttachment> (v, "stabilizer",  stabilizerBox);
    aQual  = std::make_unique<A::ComboBoxAttachment> (v, "qualityMode", qualityBox);
    aClassicFilter = std::make_unique<A::ComboBoxAttachment> (v, "classic_filter_style", classicFilterBox);

    aRet   = std::make_unique<A::SliderAttachment> (v, "retuneMs", retune);
    aStr   = std::make_unique<A::SliderAttachment> (v, "strength", strength);
    aFrm   = std::make_unique<A::SliderAttachment> (v, "formant",  formant);
    aSty   = std::make_unique<A::SliderAttachment> (v, "style",    style);

    aAutoG = std::make_unique<A::ButtonAttachment> (v, "autoGain",   autoGainBtn);
    aByp   = std::make_unique<A::ButtonAttachment> (v, "bypass",     bypassBtn);
    aSecret= std::make_unique<A::ButtonAttachment> (v, "secretMode", secretBtn);
    aClassicMode = std::make_unique<A::ButtonAttachment> (v, "classic_mode", classicModeBtn);

    // Meters
    addAndMakeVisible (meters);
    meters.start();

    startTimerHz (30);
}

PitchEngineEditor::~PitchEngineEditor()
{
    setLookAndFeel (nullptr);
}

void PitchEngineEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xFF0B0F14));

    auto r = getLocalBounds().reduced (12);
    auto headerArea = r.removeFromTop (44);
    juce::ignoreUnused (headerArea);

    // Panel
    g.setColour (juce::Colour (0xFF121823));
    g.fillRoundedRectangle (r.toFloat(), 10.0f);
    g.setColour (juce::Colour (0xFF1C2330));
    g.drawRoundedRectangle (r.toFloat(), 10.0f, 1.0f);
}

void PitchEngineEditor::resized()
{
    auto r = getLocalBounds().reduced (12);
    header.setBounds (r.removeFromTop (44));

    auto row1 = r.removeFromTop (64);  r.removeFromTop (8);
    auto row2 = r.removeFromTop (180); r.removeFromTop (8);
    auto meterRow = r.removeFromBottom (90);

    auto cell = [](juce::Rectangle<int>& row, int w){ auto c = row.removeFromLeft (w); row.removeFromLeft (8); return c.reduced (4); };

    // Row 1: menus + toggles
    keyBox       .setBounds (cell (row1, 120));
    scaleBox     .setBounds (cell (row1, 140));
    stabilizerBox.setBounds (cell (row1, 120));
    qualityBox   .setBounds (cell (row1, 120));
    classicFilterBox.setBounds (cell (row1, 120));
    autoGainBtn  .setBounds (cell (row1, 100));
    bypassBtn    .setBounds (cell (row1, 80));
    secretBtn    .setBounds (cell (row1, 90));
    classicModeBtn.setBounds (cell (row1, 90));

    // Row 2: knobs
    retune  .setBounds (cell (row2, 160));
    strength.setBounds (cell (row2, 160));
    formant .setBounds (cell (row2, 160));
    style   .setBounds (cell (row2, 160));

    // Meters
    meters.setBounds (meterRow.reduced (8));
}

void PitchEngineEditor::timerCallback()
{
    // Update header from params
    const bool print  = int (proc.apvts.getRawParameterValue ("qualityMode")->load()) == 1;
    const bool byp    = proc.apvts.getRawParameterValue ("bypass")->load() > 0.5f;
    const bool secret = proc.apvts.getRawParameterValue ("secretMode")->load() > 0.5f;

    header.setLatencyText (print ? "+48 ms" : "≤5 ms");
    header.setStates (stateA, stateB, byp, print, secret);

    // Pull meters using race-free method
    auto meterFrame = proc.readMeters();
    meters.setLevels(meterFrame.rmsL, meterFrame.rmsR,
                     meterFrame.clipL, meterFrame.clipR);
}