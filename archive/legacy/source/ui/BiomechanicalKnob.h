#pragma once

#include <JuceHeader.h>
#include <functional>

namespace FieldEngineFX::UI {

/**
 * Biomechanical control surface inspired by alien organic technology
 * Features pressure sensitivity, haptic feedback simulation, and living response patterns
 */
class BiomechanicalKnob : public juce::Component,
                          private juce::Timer {
public:
    BiomechanicalKnob();
    ~BiomechanicalKnob() override;

    // Value interface
    void setValue(float newValue, juce::NotificationType notification = juce::sendNotificationAsync);
    float getValue() const { return currentValue; }
    
    // Range configuration
    void setRange(float min, float max, float interval = 0.0f);
    void setSkewFactor(float skew) { skewFactor = skew; }
    
    // Appearance customization
    void setPrimaryColor(juce::Colour color) { primaryColor = color; }
    void setSecondaryColor(juce::Colour color) { secondaryColor = color; }
    void setOrganicComplexity(int complexity) { organicComplexity = juce::jlimit(3, 12, complexity); }
    
    // Behavior
    void setRotaryParameters(float startAngle, float endAngle);
    void setPressureSensitivity(float sensitivity) { pressureSensitivity = sensitivity; }
    void setMorphSpeed(float speed) { morphSpeed = speed; }
    
    // Callbacks
    std::function<void(float)> onValueChange;
    std::function<void(float)> onDragStart;
    std::function<void(float)> onDragEnd;

    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseEnter(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;

private:
    // Organic shape generation
    struct OrganicVertex {
        float angle;
        float radius;
        float phase;
        float frequency;
        float amplitude;
    };

    // Animation state
    struct AnimationState {
        float breathingPhase = 0.0f;
        float pulseIntensity = 0.0f;
        float morphFactor = 0.0f;
        float hoverGlow = 0.0f;
        float pressureResponse = 0.0f;
        std::vector<OrganicVertex> vertices;
    };

    // Core state
    float currentValue = 0.5f;
    float rangeMin = 0.0f;
    float rangeMax = 1.0f;
    float rangeInterval = 0.0f;
    float skewFactor = 1.0f;
    
    // Rotary parameters
    float rotaryStartAngle = -2.35619f; // -135 degrees
    float rotaryEndAngle = 2.35619f;    // 135 degrees
    
    // Interaction state
    bool isDragging = false;
    float dragStartValue = 0.0f;
    juce::Point<float> dragStartPos;
    float accumulatedDistance = 0.0f;
    
    // Appearance
    juce::Colour primaryColor{0xff00FFB7};   // Cyan glow
    juce::Colour secondaryColor{0xffFF006E}; // Magenta pulse
    int organicComplexity = 8;
    
    // Sensitivity
    float pressureSensitivity = 1.0f;
    float morphSpeed = 1.0f;
    
    // Animation
    AnimationState animState;
    void timerCallback() override;
    
    // Drawing methods
    void drawOrganicShape(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawInnerMechanism(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawEnergyField(juce::Graphics& g, juce::Rectangle<float> bounds);
    void drawValueIndicator(juce::Graphics& g, juce::Rectangle<float> bounds);
    
    // Shape generation
    void generateOrganicVertices();
    juce::Path createOrganicPath(float morphFactor);
    float getVertexRadius(const OrganicVertex& vertex, float time, float morph);
    
    // Value mapping
    float proportionToValue(float proportion) const;
    float valueToProportition(float value) const;
    float snapToInterval(float value) const;
    
    // Interaction helpers
    float getRotaryDelta(juce::Point<float> currentPos);
    float getLinearDelta(juce::Point<float> currentPos);
    void startPressureAnimation();
    void updateHoverState(bool hovering);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BiomechanicalKnob)
};

/**
 * Specialized variant for filter controls with resonance visualization
 */
class ResonantBiomechanicalKnob : public BiomechanicalKnob {
public:
    ResonantBiomechanicalKnob();
    
    void setResonance(float resonance) { this->resonance = resonance; repaint(); }
    void setFilterType(int type) { filterType = type; regenerateShape(); }
    
protected:
    void paint(juce::Graphics& g) override;
    
private:
    float resonance = 0.0f;
    int filterType = 0;
    
    void regenerateShape();
    void drawResonanceRings(juce::Graphics& g, juce::Rectangle<float> bounds);
};

} // namespace FieldEngineFX::UI
