#include "GLViralEditor.h"
#include <cmath>

// GPU-accelerated filter visualization shaders
const char* GLViralEditor::vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;
out vec2 FragCoord;

void main()
{
    gl_Position = vec4(aPos, 1.0);
    TexCoord = aTexCoord;
    FragCoord = aTexCoord;
}
)";

const char* GLViralEditor::fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;

in vec2 TexCoord;
in vec2 FragCoord;

uniform float u_time;
uniform float u_morph;
uniform float u_intensity;
uniform vec2 u_resolution;
uniform sampler1D u_spectrum;

// Z-plane filter visualization (never mentioned, just felt)
vec3 filterResponse(vec2 uv, float morph, float intensity) {
    float freq = uv.x;

    // Create multiple resonant peaks that morph
    float peak1 = intensity * exp(-50.0 * pow(freq - morph * 0.3, 2.0));
    float peak2 = intensity * exp(-30.0 * pow(freq - morph * 0.7, 2.0));
    float peak3 = intensity * 0.5 * exp(-80.0 * pow(freq - morph, 2.0));

    float response = peak1 + peak2 + peak3;

    // Add harmonic content
    response += intensity * 0.2 * sin(freq * 20.0 * morph + u_time * 2.0);

    return vec3(response);
}

// High contrast color palette
vec3 getColor(float value, float hue) {
    float sat = 0.9;
    float brightness = clamp(value, 0.0, 1.0);

    vec3 c = vec3(
        abs(sin(hue * 6.28318 + 0.0)) * sat + (1.0 - sat),
        abs(sin(hue * 6.28318 + 2.094)) * sat + (1.0 - sat),
        abs(sin(hue * 6.28318 + 4.188)) * sat + (1.0 - sat)
    );

    return c * brightness;
}

void main()
{
    vec2 uv = FragCoord;

    // Create filter response visualization
    vec3 response = filterResponse(uv, u_morph, u_intensity);

    // Add spectrum bars
    int barIndex = int(uv.x * 64.0);
    float specValue = texture(u_spectrum, float(barIndex) / 64.0).r;

    // Vertical bars that react to filter
    float barHeight = response.r + specValue * u_intensity;
    float bar = step(1.0 - barHeight, uv.y) * 0.8;

    // Color based on frequency position and morph
    float hue = uv.x + u_morph * 0.5 + u_time * 0.1;
    vec3 color = getColor(bar + response.r * 0.3, hue);

    // Add pulse effect
    float pulse = 1.0 + 0.2 * sin(u_time * 8.0 + uv.x * 10.0);
    color *= pulse;

    // High contrast adjustment
    color = pow(color, vec3(0.8));

    FragColor = vec4(color, 1.0);
}
)";

GLViralEditor::GLViralEditor(FieldEngineFXProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(600, 400);

    // Attach OpenGL context
    openGLContext.setRenderer(this);
    openGLContext.attachTo(*this);
    openGLContext.setContinuousRepainting(true);

    // Minimal control sliders - transparent overlay
    morphSlider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
    intensitySlider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
    driveSlider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
    mixSlider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);

    // Style sliders for overlay
    auto styleSlider = [](juce::Slider& slider) {
        slider.setColour(juce::Slider::trackColourId, juce::Colours::white.withAlpha(0.3f));
        slider.setColour(juce::Slider::thumbColourId, juce::Colours::white.withAlpha(0.8f));
        slider.setAlpha(0.8f);
    };

    styleSlider(*morphSlider);
    styleSlider(*intensitySlider);
    styleSlider(*driveSlider);
    styleSlider(*mixSlider);

    // Parameter connections
    morphSlider->setRange(0.0, 1.0, 0.001);
    morphSlider->setValue(0.5);
    morphSlider->onValueChange = [this]() {
        if (auto* param = audioProcessor.getAPVTS().getParameter("MORPH"))
            param->setValueNotifyingHost(morphSlider->getValue());
    };

    intensitySlider->setRange(0.0, 1.0, 0.001);
    intensitySlider->setValue(0.4);
    intensitySlider->onValueChange = [this]() {
        if (auto* param = audioProcessor.getAPVTS().getParameter("INTENSITY"))
            param->setValueNotifyingHost(intensitySlider->getValue());
    };

    driveSlider->setRange(0.1, 8.0, 0.1);
    driveSlider->setValue(1.0);
    driveSlider->onValueChange = [this]() {
        if (auto* param = audioProcessor.getAPVTS().getParameter("DRIVE"))
            param->setValueNotifyingHost(driveSlider->getValue() / 8.0f);
    };

    mixSlider->setRange(0.0, 1.0, 0.001);
    mixSlider->setValue(1.0);
    mixSlider->onValueChange = [this]() {
        if (auto* param = audioProcessor.getAPVTS().getParameter("mix"))
            param->setValueNotifyingHost(mixSlider->getValue());
    };

    addAndMakeVisible(*morphSlider);
    addAndMakeVisible(*intensitySlider);
    addAndMakeVisible(*driveSlider);
    addAndMakeVisible(*mixSlider);

    startTimerHz(60);
}

GLViralEditor::~GLViralEditor()
{
    openGLContext.detach();
}

void GLViralEditor::newOpenGLContextCreated()
{
    setupShaders();
    setupGeometry();

    glGenTextures(1, &gl.spectrumTexture);
    glBindTexture(GL_TEXTURE_1D, gl.spectrumTexture);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
}

void GLViralEditor::renderOpenGL()
{
    updateAudioData();
    updateSpectrumTexture();

    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(gl.shaderProgram);

    // Upload uniforms
    glUniform1f(gl.timeLocation, audioData.time);
    glUniform1f(gl.morphLocation, audioData.morph);
    glUniform1f(gl.intensityLocation, audioData.intensity);
    glUniform2f(gl.resolutionLocation, getWidth(), getHeight());

    // Bind spectrum texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_1D, gl.spectrumTexture);
    glUniform1i(gl.spectrumLocation, 0);

    // Render fullscreen quad
    glBindVertexArray(gl.VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void GLViralEditor::openGLContextClosing()
{
    glDeleteProgram(gl.shaderProgram);
    glDeleteVertexArrays(1, &gl.VAO);
    glDeleteBuffers(1, &gl.VBO);
    glDeleteTextures(1, &gl.spectrumTexture);
}

void GLViralEditor::paint(juce::Graphics& g)
{
    // OpenGL handles the background, just draw control labels
    g.setColour(juce::Colours::white);
    g.setFont(12.0f);

    auto drawLabel = [&](juce::Slider& slider, const juce::String& text) {
        g.drawText(text, slider.getBounds().translated(0, -18), juce::Justification::left);
    };

    drawLabel(*morphSlider, "MORPH");
    drawLabel(*intensitySlider, "INTENSITY");
    drawLabel(*driveSlider, "DRIVE");
    drawLabel(*mixSlider, "MIX");
}

void GLViralEditor::resized()
{
    auto bounds = getLocalBounds();
    int controlHeight = 20;
    int margin = 20;

    // Position controls at bottom
    auto controlArea = bounds.removeFromBottom(120).reduced(margin);

    morphSlider->setBounds(controlArea.removeFromTop(controlHeight));
    controlArea.removeFromTop(10);

    intensitySlider->setBounds(controlArea.removeFromTop(controlHeight));
    controlArea.removeFromTop(10);

    driveSlider->setBounds(controlArea.removeFromTop(controlHeight));
    controlArea.removeFromTop(10);

    mixSlider->setBounds(controlArea.removeFromTop(controlHeight));
}

void GLViralEditor::timerCallback()
{
    audioData.time += 1.0f/60.0f;
}

void GLViralEditor::updateAudioData()
{
    audioData.morph = morphSlider->getValue();
    audioData.intensity = intensitySlider->getValue();
    audioData.drive = driveSlider->getValue();
    audioData.mix = mixSlider->getValue();

    // Generate fake spectrum based on current filter settings
    for (int i = 0; i < 128; ++i) {
        float freq = (float)i / 128.0f;
        float distance = std::abs(freq - audioData.morph);
        float response = audioData.intensity * std::exp(-distance * 8.0f);
        response += 0.1f * std::sin(audioData.time * 3.0f + i * 0.1f);
        audioData.spectrum[i] = juce::jlimit(0.0f, 1.0f, response);
    }
}

void GLViralEditor::setupShaders()
{
    // Compile vertex shader
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    // Compile fragment shader
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    // Link program
    gl.shaderProgram = glCreateProgram();
    glAttachShader(gl.shaderProgram, vertexShader);
    glAttachShader(gl.shaderProgram, fragmentShader);
    glLinkProgram(gl.shaderProgram);

    // Get uniform locations
    gl.timeLocation = glGetUniformLocation(gl.shaderProgram, "u_time");
    gl.morphLocation = glGetUniformLocation(gl.shaderProgram, "u_morph");
    gl.intensityLocation = glGetUniformLocation(gl.shaderProgram, "u_intensity");
    gl.resolutionLocation = glGetUniformLocation(gl.shaderProgram, "u_resolution");
    gl.spectrumLocation = glGetUniformLocation(gl.shaderProgram, "u_spectrum");

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

void GLViralEditor::setupGeometry()
{
    // Fullscreen quad vertices
    float vertices[] = {
        -1.0f, -1.0f, 0.0f,  0.0f, 0.0f,
         1.0f, -1.0f, 0.0f,  1.0f, 0.0f,
         1.0f,  1.0f, 0.0f,  1.0f, 1.0f,
        -1.0f, -1.0f, 0.0f,  0.0f, 0.0f,
         1.0f,  1.0f, 0.0f,  1.0f, 1.0f,
        -1.0f,  1.0f, 0.0f,  0.0f, 1.0f
    };

    glGenVertexArrays(1, &gl.VAO);
    glGenBuffers(1, &gl.VBO);

    glBindVertexArray(gl.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, gl.VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
}

void GLViralEditor::updateSpectrumTexture()
{
    glBindTexture(GL_TEXTURE_1D, gl.spectrumTexture);
    glTexImage1D(GL_TEXTURE_1D, 0, GL_R32F, 128, 0, GL_RED, GL_FLOAT, audioData.spectrum.data());
}