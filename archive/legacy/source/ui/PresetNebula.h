#pragma once

#include <JuceHeader.h>
#include <vector>
#include <memory>

namespace FieldEngineFX::UI {

/**
 * 3D spherical preset browser with gravitational clustering and proximity preview
 * Presets float in space, clustering by similarity, with audio preview on approach
 */
class PresetNebula : public juce::Component,
                     public juce::OpenGLRenderer,
                     private juce::Timer {
public:
    // Preset data structure
    struct Preset {
        juce::String name;
        juce::String category;
        juce::var metadata;
        juce::Point3D<float> position;
        juce::Colour color;
        float energy = 1.0f;
        float size = 1.0f;
        bool isFactory = true;
        
        // Audio characteristics for clustering
        struct AudioFeatures {
            float brightness = 0.5f;
            float warmth = 0.5f;
            float complexity = 0.5f;
            float movement = 0.5f;
        } features;
    };

    PresetNebula();
    ~PresetNebula() override;

    // Preset management
    void addPreset(const Preset& preset);
    void removePreset(const juce::String& name);
    void clearPresets();
    void loadPresetBank(const juce::File& file);
    
    // Selection
    void selectPreset(const juce::String& name);
    Preset* getSelectedPreset() { return selectedPreset; }
    
    // Clustering
    void enableAutoClustering(bool enable) { autoClustering = enable; }
    void setClusteringStrength(float strength) { clusteringStrength = strength; }
    void recalculateClusters();
    
    // Preview
    void enableProximityPreview(bool enable) { proximityPreview = enable; }
    void setPreviewDistance(float distance) { previewDistance = distance; }
    void setPreviewMix(float mix) { previewMix = mix; }
    
    // Navigation
    void setCameraPosition(juce::Point3D<float> pos) { cameraPos = pos; }
    void lookAt(juce::Point3D<float> target);
    void zoomToPreset(const juce::String& name);
    void resetView();
    
    // Callbacks
    std::function<void(const Preset&)> onPresetSelected;
    std::function<void(const Preset&, float proximity)> onPresetProximity;
    std::function<void(const Preset&)> onPresetHover;

    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

    // OpenGL overrides
    void newOpenGLContextCreated() override;
    void openGLContextClosing() override;
    void renderOpenGL() override;

private:
    // Preset storage
    std::vector<std::unique_ptr<Preset>> presets;
    Preset* selectedPreset = nullptr;
    Preset* hoveredPreset = nullptr;
    Preset* proximityPreset = nullptr;
    
    // Clustering
    struct Cluster {
        juce::Point3D<float> center;
        std::vector<Preset*> members;
        juce::Colour color;
        float radius;
    };
    
    std::vector<Cluster> clusters;
    bool autoClustering = true;
    float clusteringStrength = 0.7f;
    
    // Camera and navigation
    juce::Point3D<float> cameraPos{0, 0, 5};
    juce::Point3D<float> cameraTarget{0, 0, 0};
    juce::Point3D<float> cameraUp{0, 1, 0};
    float cameraFOV = 45.0f;
    
    // Interaction state
    bool isDragging = false;
    juce::Point<float> lastMousePos;
    float rotationX = 0.0f;
    float rotationY = 0.0f;
    float zoom = 1.0f;
    
    // Preview
    bool proximityPreview = true;
    float previewDistance = 2.0f;
    float previewMix = 0.5f;
    float currentPreviewMix = 0.0f;
    
    // OpenGL resources
    juce::OpenGLContext openGLContext;
    std::unique_ptr<juce::OpenGLShaderProgram> nebulaShader;
    std::unique_ptr<juce::OpenGLShaderProgram> presetShader;
    std::unique_ptr<juce::OpenGLShaderProgram> connectionShader;
    
    struct GPUPreset {
        GLuint vbo = 0;
        GLuint vao = 0;
        int vertexCount = 0;
    };
    
    std::map<Preset*, GPUPreset> gpuPresets;
    
    // Animation
    void timerCallback() override;
    float animationTime = 0.0f;
    
    // Rendering methods
    void compileShaders();
    void createPresetGeometry(Preset* preset);
    void updatePresetPositions(float deltaTime);
    void renderNebula();
    void renderPresets();
    void renderConnections();
    void renderUI();
    
    // Clustering algorithms
    void performKMeansClustering();
    void applyGravitationalForces(float deltaTime);
    float calculatePresetSimilarity(const Preset& a, const Preset& b);
    
    // Interaction helpers
    Preset* getPresetAtScreenPos(juce::Point<float> screenPos);
    juce::Point3D<float> screenToWorld(juce::Point<float> screenPos);
    juce::Point<float> worldToScreen(juce::Point3D<float> worldPos);
    
    // Matrix operations
    juce::Matrix3D<float> getProjectionMatrix() const;
    juce::Matrix3D<float> getViewMatrix() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetNebula)
};

/**
 * Preset metadata editor with alien glyph visualization
 */
class PresetGlyphEditor : public juce::Component {
public:
    PresetGlyphEditor();
    
    void setPreset(PresetNebula::Preset* preset);
    void setEditMode(bool canEdit) { editable = canEdit; }
    
    // Glyph system
    void generateGlyph();
    juce::Path getGlyphPath() const { return glyphPath; }
    
private:
    PresetNebula::Preset* currentPreset = nullptr;
    bool editable = false;
    juce::Path glyphPath;
    
    void paint(juce::Graphics& g) override;
    void generateGlyphFromFeatures(const PresetNebula::Preset::AudioFeatures& features);
};

} // namespace FieldEngineFX::UI
