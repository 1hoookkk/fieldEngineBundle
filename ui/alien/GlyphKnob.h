#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "EMUColorPalette.h"
#include "AlienGlyphs.h"

namespace AlienUI
{
    class GlyphKnob : public juce::Slider
    {
    public:
        GlyphKnob(const juce::String& name = juce::String());
        ~GlyphKnob() override = default;
        
        // Customization
        void setGlyph(const juce::String& glyph) { customGlyph = glyph; repaint(); }
        void setGlyphFromParameter() { customGlyph = Glyphs::getParameterGlyph(getName()); repaint(); }
        void setEnergyLevel(float level) { energyLevel = juce::jlimit(0.0f, 1.0f, level); repaint(); }
        void setPulseEnabled(bool enabled) { pulseEnabled = enabled; }
        void setInteractionGlow(bool enabled) { interactionGlowEnabled = enabled; }
        
        // Value display customization
        void setValueSuffix(const juce::String& suffix) { valueSuffix = suffix; }
        void setValuePrefix(const juce::String& prefix) { valuePrefix = prefix; }
        void setDecimalPlaces(int places) { decimalPlaces = places; }
        
        // Animation
        void startPulse();
        void stopPulse();
        
    protected:
        void paint(juce::Graphics& g) override;
        void mouseEnter(const juce::MouseEvent& event) override;
        void mouseExit(const juce::MouseEvent& event) override;
        void mouseDown(const juce::MouseEvent& event) override;
        void mouseUp(const juce::MouseEvent& event) override;
        void mouseDrag(const juce::MouseEvent& event) override;
        
    private:
        void timerCallback();
        void updateGlowIntensity();
        juce::String formatValue(double value) const;
        
        // Visual state
        juce::String customGlyph;
        float energyLevel = 0.0f;
        float glowIntensity = 0.0f;
        float pulsePhase = 0.0f;
        bool pulseEnabled = true;
        bool interactionGlowEnabled = true;
        bool isInteracting = false;
        
        // Value formatting
        juce::String valueSuffix;
        juce::String valuePrefix;
        int decimalPlaces = 1;
        
        // Animation timer
        class AnimationTimer : public juce::Timer
        {
        public:
            std::function<void()> onTimerCallback;
            void timerCallback() override { if (onTimerCallback) onTimerCallback(); }
        };
        
        AnimationTimer animationTimer;
        
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GlyphKnob)
    };
    
    // Specialized knob variants
    class FrequencyKnob : public GlyphKnob
    {
    public:
        FrequencyKnob();
        juce::String getTextFromValue(double value) override;
        double getValueFromText(const juce::String& text) override;
    };
    
    class ResonanceKnob : public GlyphKnob
    {
    public:
        ResonanceKnob();
        juce::String getTextFromValue(double value) override;
    };
    
    class MorphKnob : public GlyphKnob
    {
    public:
        MorphKnob();
        void setMorphLabels(const juce::String& startLabel, const juce::String& endLabel);
        juce::String getTextFromValue(double value) override;
        
    private:
        juce::String morphStartLabel = "LP";
        juce::String morphEndLabel = "HP";
    };
}
