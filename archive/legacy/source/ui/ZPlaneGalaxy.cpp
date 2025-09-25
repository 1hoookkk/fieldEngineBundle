#include "ZPlaneGalaxy.h"

namespace FieldEngineFX::UI {

// Vertex shader for constellation rendering
static const char* constellationVertexShader = R"(
#version 330 core
layout(location = 0) in vec3 position;
layout(location = 1) in float energy;

uniform mat4 projectionMatrix;
uniform mat4 viewMatrix;
uniform float time;
uniform float morphPosition;

out vec4 fragColor;
out vec2 texCoord;
out float glowIntensity;

// Gravitational wave distortion
vec3 gravitationalDistortion(vec3 pos, float t) {
    float wave = sin(length(pos.xy) * 3.14159 - t * 2.0) * 0.1;
    float spiral = atan(pos.y, pos.x) + t * 0.5;
    vec2 distortion = vec2(cos(spiral), sin(spiral)) * wave * morphPosition;
    return vec3(pos.xy + distortion, pos.z);
}

void main() {
    vec3 distortedPos = gravitationalDistortion(position, time);
    gl_Position = projectionMatrix * viewMatrix * vec4(distortedPos, 1.0);
    
    // Energy-based coloring
    float pulse = sin(time * 3.0 + energy * 6.28318) * 0.5 + 0.5;
    glowIntensity = energy * pulse;
    
    // Bioluminescent color gradient
    vec3 cyanGlow = vec3(0.0, 1.0, 0.714);  // #00FFB7
    vec3 magentaPulse = vec3(1.0, 0.0, 0.431); // #FF006E
    fragColor = vec4(mix(cyanGlow, magentaPulse, energy), glowIntensity);
    
    texCoord = position.xy * 0.5 + 0.5;
}
)";

// Fragment shader for constellation rendering
static const char* constellationFragmentShader = R"(
#version 330 core
in vec4 fragColor;
in vec2 texCoord;
in float glowIntensity;

out vec4 outputColor;

uniform float time;
uniform float resonanceEnergy;

// Hexagonal pattern generation
float hexagon(vec2 p, float r) {
    const vec3 k = vec3(-0.866025404, 0.5, 0.577350269);
    p = abs(p);
    p -= 2.0 * min(dot(k.xy, p), 0.0) * k.xy;
    p -= vec2(clamp(p.x, -k.z * r, k.z * r), r);
    return length(p) * sign(p.y);
}

// Quantum fluctuation noise
float quantumNoise(vec2 p, float t) {
    return fract(sin(dot(p, vec2(12.9898, 78.233)) + t) * 43758.5453);
}

void main() {
    // Core glow with hexagonal structure
    float hexDist = hexagon(texCoord - 0.5, 0.1);
    float glow = exp(-hexDist * hexDist * 10.0) * glowIntensity;
    
    // Quantum fluctuations
    float noise = quantumNoise(texCoord, time) * 0.1;
    glow += noise * resonanceEnergy;
    
    // Energy field lines
    float fieldLines = sin(length(texCoord - 0.5) * 20.0 - time * 3.0) * 0.1;
    glow += fieldLines * resonanceEnergy;
    
    // Final color with HDR bloom
    vec3 color = fragColor.rgb * glow;
    color = pow(color, vec3(0.8)); // Gamma correction
    
    outputColor = vec4(color, glow * fragColor.a);
}
)";

// Gravitational wave shader
static const char* waveVertexShader = R"(
#version 330 core
layout(location = 0) in vec2 position;

uniform mat4 projectionMatrix;
uniform float time;

out vec2 uv;

void main() {
    gl_Position = projectionMatrix * vec4(position, 0.0, 1.0);
    uv = position;
}
)";

static const char* waveFragmentShader = R"(
#version 330 core
in vec2 uv;
out vec4 outputColor;

uniform float time;
uniform vec2 polePositions[16];
uniform float poleStrengths[16];
uniform int activePoles;

// Gravitational wave field calculation
float gravitationalField(vec2 pos) {
    float field = 0.0;
    for (int i = 0; i < activePoles; i++) {
        float dist = length(pos - polePositions[i]);
        float wave = sin(dist * 10.0 - time * 3.0) * exp(-dist * 2.0);
        field += wave * poleStrengths[i];
    }
    return field;
}

void main() {
    float field = gravitationalField(uv);
    
    // Visualize as interference pattern
    vec3 color = vec3(0.0);
    color.r = max(0.0, field) * 0.5;
    color.b = max(0.0, -field) * 0.5;
    color.g = abs(field) * 0.2;
    
    // Add grid distortion
    vec2 grid = fract(uv * 10.0);
    float gridLine = smoothstep(0.98, 1.0, max(grid.x, grid.y));
    color += vec3(0.1, 0.2, 0.3) * gridLine * (1.0 + field * 0.5);
    
    outputColor = vec4(color, abs(field) * 0.3);
}
)";

ZPlaneGalaxy::ZPlaneGalaxy() {
    openGLContext.setOpenGLVersionRequired(juce::OpenGLContext::openGL3_2);
    openGLContext.setRenderer(this);
    openGLContext.attachTo(*this);
    
    startTimerHz(60); // 60 FPS animation
}

ZPlaneGalaxy::~ZPlaneGalaxy() {
    openGLContext.detach();
}

void ZPlaneGalaxy::setCoefficients(const ZPlaneState& state) {
    StateProvider::GalaxyState galaxyState;
    
    // Convert filter coefficients to pole/zero positions
    for (int i = 0; i < state.poles.size() && i < 16; ++i) {
        galaxyState.poles[i] = state.poles[i];
    }
    
    for (int i = 0; i < state.zeros.size() && i < 16; ++i) {
        galaxyState.zeros[i] = state.zeros[i];
    }
    
    galaxyState.resonanceEnergy = state.resonance;
    galaxyState.cutoffPhase = state.cutoff * juce::MathConstants<float>::twoPi;
    
    stateProvider.pushState(galaxyState);
}

void ZPlaneGalaxy::setMorphTrajectory(const juce::Path& path) {
    // Store morph path for interpolation
    // Implementation would convert path to GPU-friendly format
}

void ZPlaneGalaxy::setEnergyLevels(const std::array<float, 8>& levels) {
    // Update particle system energy distribution
    for (int i = 0; i < particles.MAX_PARTICLES; ++i) {
        int band = i % 8;
        particles.lifetimes[i] = levels[band];
    }
}

void ZPlaneGalaxy::paint(juce::Graphics& g) {
    // Fallback 2D rendering if OpenGL fails
    if (!openGLContext.isActive()) {
        g.fillAll(juce::Colour(0x0A0E1B)); // void-black
        g.setColour(juce::Colours::white);
        g.drawText("OpenGL Context Failed", getLocalBounds(), 
                   juce::Justification::centred);
    }
}

void ZPlaneGalaxy::resized() {
    // Update viewport
}

void ZPlaneGalaxy::newOpenGLContextCreated() {
    compileShaders();
    
    // Initialize particle system
    particles.positions.resize(particles.MAX_PARTICLES);
    particles.velocities.resize(particles.MAX_PARTICLES);
    particles.lifetimes.resize(particles.MAX_PARTICLES);
    
    // Create VAO/VBO for particles
    openGLContext.extensions.glGenVertexArrays(1, &particles.vao);
    openGLContext.extensions.glGenBuffers(1, &particles.vbo);
    
    // Initialize particles in galaxy formation
    for (int i = 0; i < particles.MAX_PARTICLES; ++i) {
        float angle = (float)i / particles.MAX_PARTICLES * juce::MathConstants<float>::twoPi * 8.0f;
        float radius = std::sqrt((float)i / particles.MAX_PARTICLES) * 2.0f;
        
        particles.positions[i] = {
            std::cos(angle) * radius,
            std::sin(angle) * radius,
            (juce::Random::getSystemRandom().nextFloat() - 0.5f) * 0.1f
        };
        
        particles.velocities[i] = {
            (juce::Random::getSystemRandom().nextFloat() - 0.5f) * 0.01f,
            (juce::Random::getSystemRandom().nextFloat() - 0.5f) * 0.01f,
            0.0f
        };
        
        particles.lifetimes[i] = juce::Random::getSystemRandom().nextFloat();
    }
}

void ZPlaneGalaxy::openGLContextClosing() {
    // Clean up GPU resources
    if (particles.vao != 0) {
        openGLContext.extensions.glDeleteVertexArrays(1, &particles.vao);
        particles.vao = 0;
    }
    
    if (particles.vbo != 0) {
        openGLContext.extensions.glDeleteBuffers(1, &particles.vbo);
        particles.vbo = 0;
    }
}

void ZPlaneGalaxy::renderOpenGL() {
    juce::ScopedNVGFrame frame(frameCounter);
    
    // Clear with deep space color
    juce::OpenGLHelpers::clear(juce::Colour(0xff0A0E1B));
    
    // Enable blending for transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Render layers in order
    renderGravitationalWaves();
    renderConstellations();
    renderEnergyField();
    
    // Update performance counter
    lastRenderTime = frameCounter.getTimeTaken();
}

void ZPlaneGalaxy::compileShaders() {
    // Compile constellation shader
    constellationShader.program = std::make_unique<juce::OpenGLShaderProgram>(openGLContext);
    
    if (constellationShader.program->addVertexShader(constellationVertexShader)
        && constellationShader.program->addFragmentShader(constellationFragmentShader)
        && constellationShader.program->link()) {
        
        constellationShader.timeUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform>(
            *constellationShader.program, "time");
        constellationShader.energyUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform>(
            *constellationShader.program, "resonanceEnergy");
        constellationShader.morphUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform>(
            *constellationShader.program, "morphPosition");
    }
    
    // Compile wave shader
    waveShader.program = std::make_unique<juce::OpenGLShaderProgram>(openGLContext);
    
    if (waveShader.program->addVertexShader(waveVertexShader)
        && waveShader.program->addFragmentShader(waveFragmentShader)
        && waveShader.program->link()) {
        
        waveShader.timeUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform>(
            *waveShader.program, "time");
    }
}

void ZPlaneGalaxy::timerCallback() {
    // Trigger OpenGL repaint
    openGLContext.triggerRepaint();
    
    // Update particle physics
    updateParticles(1.0f / 60.0f);
}

void ZPlaneGalaxy::updateParticles(float deltaTime) {
    StateProvider::GalaxyState state;
    if (!stateProvider.pullState(state)) {
        return;
    }
    
    // Apply gravitational forces from poles
    for (int i = 0; i < particles.MAX_PARTICLES; ++i) {
        auto& pos = particles.positions[i];
        auto& vel = particles.velocities[i];
        
        // Calculate forces from active poles
        juce::Point3D<float> force{0, 0, 0};
        
        for (int p = 0; p < state.poles.size(); ++p) {
            if (std::abs(state.poles[p]) < 0.001f) continue;
            
            juce::Point3D<float> polePos{
                state.poles[p].real(),
                state.poles[p].imag(),
                0.0f
            };
            
            auto delta = polePos - pos;
            float distSq = delta.x * delta.x + delta.y * delta.y + 0.01f;
            float strength = gravitationalStrength.load() / distSq;
            
            force += delta * strength;
        }
        
        // Apply quantum fluctuations
        if (quantumFluctuations.load()) {
            force.x += (juce::Random::getSystemRandom().nextFloat() - 0.5f) * 0.001f;
            force.y += (juce::Random::getSystemRandom().nextFloat() - 0.5f) * 0.001f;
        }
        
        // Update velocity and position
        vel += force * deltaTime;
        vel *= 0.98f; // Damping
        pos += vel * deltaTime;
        
        // Lifetime decay
        particles.lifetimes[i] -= deltaTime * 0.1f;
        if (particles.lifetimes[i] <= 0.0f) {
            particles.lifetimes[i] = 1.0f;
            // Respawn at edge of galaxy
            float angle = juce::Random::getSystemRandom().nextFloat() * juce::MathConstants<float>::twoPi;
            pos = {std::cos(angle) * 2.0f, std::sin(angle) * 2.0f, 0.0f};
        }
    }
}

void ZPlaneGalaxy::renderConstellations() {
    // Implementation of constellation rendering using the compiled shaders
    if (constellationShader.program == nullptr) return;
    
    constellationShader.program->use();
    
    if (constellationShader.timeUniform != nullptr) {
        constellationShader.timeUniform->set(static_cast<float>(juce::Time::currentTimeMillis()) / 1000.0f);
    }
    
    // Render pole/zero constellations
    // ... GPU rendering code ...
}

void ZPlaneGalaxy::renderGravitationalWaves() {
    // Implementation of gravitational wave visualization
    if (waveShader.program == nullptr) return;
    
    waveShader.program->use();
    
    if (waveShader.timeUniform != nullptr) {
        waveShader.timeUniform->set(static_cast<float>(juce::Time::currentTimeMillis()) / 1000.0f);
    }
    
    // Render wave distortion field
    // ... GPU rendering code ...
}

void ZPlaneGalaxy::renderEnergyField() {
    // Render particle system as energy field
    // ... GPU particle rendering ...
}

// StateProvider implementation
void ZPlaneGalaxy::StateProvider::pushState(const GalaxyState& state) {
    const juce::SpinLock::ScopedLockType lock(stateLock);
    currentState = state;
    hasNewState = true;
}

bool ZPlaneGalaxy::StateProvider::pullState(GalaxyState& state) {
    if (!hasNewState.load()) return false;
    
    const juce::SpinLock::ScopedLockType lock(stateLock);
    state = currentState;
    hasNewState = false;
    return true;
}

} // namespace FieldEngineFX::UI
