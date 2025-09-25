#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_opengl/juce_opengl.h>
#include "../fx/FieldEngineFXProcessor.h"
#include <array>

class GLViralEditor : public juce::AudioProcessorEditor,
                      public juce::OpenGLRenderer,
                      public juce::Timer
{
public:
    GLViralEditor(FieldEngineFXProcessor&);
    ~GLViralEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

    // OpenGL callbacks
    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;

private:
    FieldEngineFXProcessor& audioProcessor;
    juce::OpenGLContext openGLContext;

    // GPU-accelerated visual data
    struct AudioData {
        std::array<float, 128> spectrum{};
        float morph = 0.5f;
        float intensity = 0.4f;
        float drive = 1.0f;
        float mix = 1.0f;
        float time = 0.0f;
    } audioData;

    // Minimal control overlays (drawn with regular JUCE)
    std::unique_ptr<juce::Slider> morphSlider;
    std::unique_ptr<juce::Slider> intensitySlider;
    std::unique_ptr<juce::Slider> driveSlider;
    std::unique_ptr<juce::Slider> mixSlider;

    // OpenGL resources
    struct GLResources {
        unsigned int shaderProgram = 0;
        unsigned int VAO = 0;
        unsigned int VBO = 0;
        unsigned int spectrumTexture = 0;
        int morphLocation = -1;
        int intensityLocation = -1;
        int timeLocation = -1;
        int resolutionLocation = -1;
        int spectrumLocation = -1;
    } gl;

    void updateAudioData();
    void setupShaders();
    void setupGeometry();
    void updateSpectrumTexture();

    // Shader source
    static const char* vertexShaderSource;
    static const char* fragmentShaderSource;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GLViralEditor)
};