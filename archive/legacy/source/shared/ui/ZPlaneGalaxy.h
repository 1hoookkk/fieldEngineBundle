#pragma once

#include <JuceHeader.h>
#include <array>
#include <complex>
#include "FilterState.h"

namespace FieldEngineFX::UI {

/**
 * GPU-accelerated visualization of Z-plane filter coefficients as a living constellation
 * Renders poles and zeros as gravitationally-bound energy nodes with morphing trajectories
 */
class ZPlaneGalaxy : public juce::Component,
                     public juce::OpenGLRenderer,
                     private juce::Timer {
public:
    ZPlaneGalaxy();
    ~ZPlaneGalaxy() override;

    // Core Interface
    void setCoefficients(const ZPlaneState& state);
    void setMorphTrajectory(const juce::Path& path);
    void setEnergyLevels(const std::array<float, 8>& levels);
    
    // Animation Control
    void setGravitationalStrength(float strength);
    void setParticleCount(int count);
    void enableQuantumFluctuations(bool enable);

    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

    // OpenGL callbacks
    void newOpenGLContextCreated() override;
    void openGLContextClosing() override;
    void renderOpenGL() override;

    // Real-time safe state provider
    class StateProvider {
    public:
        struct GalaxyState {
            std::array<std::complex<float>, 16> poles;
            std::array<std::complex<float>, 16> zeros;
            float morphPosition = 0.0f;
            float resonanceEnergy = 0.0f;
            float cutoffPhase = 0.0f;
        };

        void pushState(const GalaxyState& state);
        bool pullState(GalaxyState& state);

    private:
        std::atomic<bool> hasNewState{false};
        GalaxyState currentState;
        juce::SpinLock stateLock;
    };

private:
    // GPU Resources
    struct ShaderProgram {
        std::unique_ptr<juce::OpenGLShaderProgram> program;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> timeUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> energyUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> morphUniform;
    };

    struct ParticleSystem {
        static constexpr int MAX_PARTICLES = 2048;
        std::vector<juce::Point3D<float>> positions;
        std::vector<juce::Point3D<float>> velocities;
        std::vector<float> lifetimes;
        GLuint vbo = 0;
        GLuint vao = 0;
    };

    // Rendering pipeline
    void compileShaders();
    void updateParticles(float deltaTime);
    void renderConstellations();
    void renderGravitationalWaves();
    void renderEnergyField();

    // Animation
    void timerCallback() override;
    float easeInOutCubic(float t);
    juce::Point<float> gravitationalLens(juce::Point<float> point, float time);

    // State Management
    StateProvider stateProvider;
    std::atomic<float> gravitationalStrength{1.0f};
    std::atomic<bool> quantumFluctuations{true};
    
    // OpenGL Context
    juce::OpenGLContext openGLContext;
    ShaderProgram constellationShader;
    ShaderProgram waveShader;
    ShaderProgram particleShader;
    ParticleSystem particles;
    
    // Performance Monitoring
    juce::PerformanceCounter frameCounter{"ZPlaneGalaxy FPS"};
    std::atomic<float> lastRenderTime{0.0f};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ZPlaneGalaxy)
};

} // namespace FieldEngineFX::UI
