#pragma once

#include <JuceHeader.h>
#include <vector>
#include <array>
#include <atomic>

namespace FieldEngineFX::UI {

class EnergyFlowVisualizer : public juce::Component,
                              private juce::Timer {
public:
    EnergyFlowVisualizer();
    ~EnergyFlowVisualizer() override;
    
    void pushAudioData(const float* data, int numSamples);
    void setFlowDirection(float angle) { flowDirection = angle; }
    void setParticleCount(int count) { particleCount = juce::jlimit(100, 5000, count); }
    
    void paint(juce::Graphics& g) override;
    
private:
    struct Particle {
        juce::Point<float> position;
        juce::Point<float> velocity;
        float energy;
        float lifetime;
        juce::Colour color;
    };
    
    std::vector<Particle> particles;
    int particleCount = 1000;
    float flowDirection = 0.0f;
    
    std::array<float, 512> audioBuffer;
    std::atomic<int> writeIndex{0};
    float currentEnergy = 0.0f;
    float peakFrequency = 0.0f;
    
    void timerCallback() override;
    void updateParticles();
    void spawnParticle();
    void analyzeAudio();
};

} // namespace FieldEngineFX::UI
