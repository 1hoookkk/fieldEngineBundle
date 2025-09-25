#pragma once
#include <JuceHeader.h>
#include <memory>
#include <atomic>
#include <vector>
#include <array>

/**
 * Visual Feedback Engine - The Satisfying Visual Experience
 * 
 * Creates the satisfying, functional visual feedback that makes SPECTRAL CANVAS PRO
 * feel like those classic early 2000s applications but with modern polish.
 * 
 * Core Features:
 * - Real-time 3D audio visualization with depth and movement
 * - Paint stroke visualization with trails and glow effects
 * - Frequency spectrum analysis with linear drumming visualization
 * - Satisfying visual feedback for every interaction
 * - Performance-optimized GPU-accelerated rendering
 * - Professional information density without clutter
 */
class VisualFeedbackEngine
{
public:
    VisualFeedbackEngine();
    ~VisualFeedbackEngine();
    
    //==============================================================================
    // Core Visual System
    
    void initialize(juce::OpenGLContext* glContext = nullptr);
    void shutdown();
    void renderFrame(juce::Graphics& g, juce::Rectangle<int> bounds);
    
    // Update with audio data
    void updateAudioData(const juce::AudioBuffer<float>& buffer);
    void updateSpectrumData(const std::vector<float>& magnitudeSpectrum);
    void updateTrackerData(const std::vector<std::vector<int>>& patternData);
    
    //==============================================================================
    // Paint Stroke Visualization
    
    struct PaintTrail
    {
        juce::Path strokePath;
        juce::Colour color;
        float intensity = 1.0f;
        float age = 0.0f;           // Age in seconds
        float maxAge = 2.0f;        // Fade out time
        bool isActive = true;
        
        // Visual effects
        float glowRadius = 5.0f;
        float strokeWidth = 2.0f;
        bool hasParticles = false;
        std::vector<juce::Point<float>> particles;
        
        void update(float deltaTime);
        void render(juce::Graphics& g, const juce::AffineTransform& transform) const;
    };
    
    // Paint stroke management
    void addPaintStroke(const juce::Path& path, juce::Colour color, float intensity);
    void clearPaintStrokes();
    void setPaintTrailLength(float seconds) { paintTrailLength.store(seconds); }
    void enablePaintGlow(bool enable) { paintGlowEnabled.store(enable); }
    void enablePaintParticles(bool enable) { paintParticlesEnabled.store(enable); }
    
    //==============================================================================
    // 3D Audio Visualization
    
    enum class VisualizationMode
    {
        Spectrum2D,          // Classic 2D spectrum analyzer
        Spectrum3D,          // 3D rotating spectrum with depth
        Waveform3D,          // 3D waveform visualization
        FrequencyBars,       // Linear drumming frequency bars
        SpectrumSphere,      // Spherical spectrum visualization
        SpectrumTunnel,      // Tunnel effect with spectrum
        ParticleField,       // Audio-reactive particle system
        VinylSpectrum        // Rotating vinyl-style visualization
    };
    
    void setVisualizationMode(VisualizationMode mode) { currentVisualizationMode.store(static_cast<int>(mode)); }
    VisualizationMode getVisualizationMode() const { return static_cast<VisualizationMode>(currentVisualizationMode.load()); }
    
    // 3D visualization parameters
    struct Visualization3DParams
    {
        float rotationSpeed = 0.5f;     // Rotation speed multiplier
        float cameraDistance = 5.0f;    // Camera distance from center
        float perspectiveAngle = 45.0f; // Field of view
        bool autoRotate = true;         // Automatic rotation
        juce::Vector3D<float> cameraPosition{0.0f, 0.0f, 5.0f};
        juce::Vector3D<float> lookAtPoint{0.0f, 0.0f, 0.0f};
        
        // Visual style
        juce::Colour baseColor = juce::Colours::cyan;
        juce::Colour accentColor = juce::Colours::magenta;
        float transparency = 0.8f;
        bool wireframeMode = false;
        bool showGrid = true;
    };
    
    void set3DParams(const Visualization3DParams& params) { visualization3DParams = params; }
    Visualization3DParams get3DParams() const { return visualization3DParams; }
    
    //==============================================================================
    // Frequency Analysis Visualization
    
    struct FrequencyVisualization
    {
        static constexpr int NUM_BANDS = 128;
        std::array<float, NUM_BANDS> magnitudes{};
        std::array<float, NUM_BANDS> phases{};
        std::array<float, NUM_BANDS> peakValues{};
        std::array<float, NUM_BANDS> peakAges{};
        
        // Linear drumming frequency ranges
        struct FrequencyBand
        {
            float lowFreq;
            float highFreq;
            float currentLevel;
            float peakLevel;
            juce::Colour bandColor;
            juce::String bandName;
            bool isActive = false;
        };
        
        std::array<FrequencyBand, 16> drumFrequencyBands; // For 16 tracker tracks
        
        void updateFromSpectrum(const std::vector<float>& spectrum, float sampleRate);
        void updatePeaks(float deltaTime);
        void renderBands(juce::Graphics& g, juce::Rectangle<int> bounds);
    };
    
    FrequencyVisualization& getFrequencyVisualization() { return frequencyVisualization; }
    
    //==============================================================================
    // Tracker Pattern Visualization
    
    struct TrackerVisualization
    {
        static constexpr int MAX_TRACKS = 16;
        static constexpr int MAX_ROWS = 64;
        
        struct TrackerCell
        {
            bool hasNote = false;
            float intensity = 0.0f;     // 0.0-1.0
            juce::Colour cellColor = juce::Colours::white;
            float age = 0.0f;           // For fade effects
            bool isPlaying = false;     // Currently being played
        };
        
        std::array<std::array<TrackerCell, MAX_ROWS>, MAX_TRACKS> cells;
        int currentPlayRow = 0;
        float rowHighlightPosition = 0.0f;
        
        // Visual style
        juce::Colour trackColors[MAX_TRACKS];
        float cellSpacing = 2.0f;
        float trackWidth = 30.0f;
        bool showTrackNames = true;
        bool showRowNumbers = true;
        
        void updateFromPattern(const std::vector<std::vector<int>>& pattern);
        void setPlaybackPosition(int row, float subRowPosition);
        void renderPattern(juce::Graphics& g, juce::Rectangle<int> bounds);
    };
    
    TrackerVisualization& getTrackerVisualization() { return trackerVisualization; }
    
    //==============================================================================
    // Real-Time Visual Effects
    
    // Screen shake for impact
    void triggerScreenShake(float intensity = 1.0f, float duration = 0.2f);
    void enableScreenShake(bool enable) { screenShakeEnabled.store(enable); }
    
    // Flash effects for accents
    void triggerFlash(juce::Colour color = juce::Colours::white, float intensity = 0.5f, float duration = 0.1f);
    void enableFlashEffects(bool enable) { flashEffectsEnabled.store(enable); }
    
    // Chromatic aberration for intensity
    void setChromaticAberration(float amount) { chromaticAberrationAmount.store(amount); }
    void enableChromaticAberration(bool enable) { chromaticAberrationEnabled.store(enable); }
    
    // Particle systems
    void createParticleSystem(juce::Point<float> origin, juce::Colour color, int count = 20);
    void enableParticleEffects(bool enable) { particleEffectsEnabled.store(enable); }
    
    //==============================================================================
    // Performance & Quality
    
    enum class QualityLevel
    {
        Performance = 0,    // Basic visuals, max performance
        Balanced = 1,       // Good balance of quality and performance
        Quality = 2,        // Maximum visual quality
        Ultra = 3          // All effects enabled, may impact performance
    };
    
    void setQualityLevel(QualityLevel level);
    QualityLevel getQualityLevel() const { return static_cast<QualityLevel>(currentQualityLevel.load()); }
    
    // Performance metrics
    struct PerformanceMetrics
    {
        float averageFPS = 60.0f;
        float frameTimeMs = 16.67f;
        int droppedFrames = 0;
        float gpuUsage = 0.0f;      // If GPU acceleration available
        int activeParticles = 0;
        int activePaintTrails = 0;
    };
    
    PerformanceMetrics getPerformanceMetrics() const;
    void resetPerformanceCounters();
    
    //==============================================================================
    // Color Schemes & Themes
    
    enum class ColorScheme
    {
        Classic,            // Classic early 2000s colors
        Modern,             // Modern professional colors
        Neon,               // Bright neon colors
        Retro,              // Retro synthwave colors
        Minimal,            // Minimal grayscale
        Custom              // User-defined colors
    };
    
    struct ColorTheme
    {
        juce::Colour backgroundColor = juce::Colour(0xff1a1a1a);
        juce::Colour foregroundColor = juce::Colour(0xffffffff);
        juce::Colour accentColor = juce::Colour(0xff00ffff);
        juce::Colour gridColor = juce::Colour(0xff333333);
        juce::Colour textColor = juce::Colour(0xffcccccc);
        juce::Colour highlightColor = juce::Colour(0xffffff00);
        
        // Spectrum colors
        juce::Colour spectrumLow = juce::Colour(0xff0000ff);     // Blue for low frequencies
        juce::Colour spectrumMid = juce::Colour(0xff00ff00);     // Green for mid frequencies
        juce::Colour spectrumHigh = juce::Colour(0xffff0000);    // Red for high frequencies
        
        // Paint stroke colors
        std::vector<juce::Colour> paintColors = {
            juce::Colour(0xffff0000), juce::Colour(0xff00ff00), juce::Colour(0xff0000ff),
            juce::Colour(0xffffff00), juce::Colour(0xffff00ff), juce::Colour(0xff00ffff)
        };
    };
    
    void setColorScheme(ColorScheme scheme);
    void setCustomColorTheme(const ColorTheme& theme);
    ColorTheme getCurrentColorTheme() const { return currentColorTheme; }
    
private:
    //==============================================================================
    // Rendering Engine
    
    // TODO: Implement actual renderer classes
    // class OpenGLRenderer;
    // class SoftwareRenderer;
    
    // std::unique_ptr<OpenGLRenderer> openGLRenderer;
    // std::unique_ptr<SoftwareRenderer> softwareRenderer;
    bool useOpenGL = false;
    
    //==============================================================================
    // Visual Data
    
    std::vector<PaintTrail> activePaintTrails;
    FrequencyVisualization frequencyVisualization;
    TrackerVisualization trackerVisualization;
    Visualization3DParams visualization3DParams;
    ColorTheme currentColorTheme;
    
    //==============================================================================
    // Real-Time Effects State
    
    struct ScreenShake
    {
        bool isActive = false;
        float intensity = 0.0f;
        float duration = 0.0f;
        float timeRemaining = 0.0f;
        juce::Random random;
        
        juce::Point<float> getCurrentOffset();
        void update(float deltaTime);
    } screenShake;
    
    struct FlashEffect
    {
        bool isActive = false;
        juce::Colour color = juce::Colours::white;
        float intensity = 0.0f;
        float duration = 0.0f;
        float timeRemaining = 0.0f;
        
        void update(float deltaTime);
        float getCurrentAlpha() const;
    } flashEffect;
    
    struct Particle
    {
        juce::Point<float> position;
        juce::Point<float> velocity;
        juce::Colour color;
        float life = 1.0f;
        float maxLife = 1.0f;
        float size = 1.0f;
        
        void update(float deltaTime);
        bool isAlive() const { return life > 0.0f; }
    };
    
    std::vector<Particle> activeParticles;
    
    //==============================================================================
    // State Management
    
    std::atomic<int> currentVisualizationMode{0};
    std::atomic<int> currentQualityLevel{1}; // Balanced by default
    std::atomic<float> paintTrailLength{2.0f};
    
    // Effect toggles
    std::atomic<bool> paintGlowEnabled{true};
    std::atomic<bool> paintParticlesEnabled{true};
    std::atomic<bool> screenShakeEnabled{true};
    std::atomic<bool> flashEffectsEnabled{true};
    std::atomic<bool> chromaticAberrationEnabled{false};
    std::atomic<bool> particleEffectsEnabled{true};
    
    std::atomic<float> chromaticAberrationAmount{0.0f};
    
    //==============================================================================
    // Timing & Animation
    
    juce::Time lastFrameTime;
    float deltaTime = 0.016f; // ~60 FPS
    float currentTime = 0.0f;
    
    // Animation state
    float rotationAngle = 0.0f;
    float pulsePhase = 0.0f;
    float wavePhase = 0.0f;
    
    //==============================================================================
    // Performance Tracking
    
    mutable PerformanceMetrics performanceMetrics;
    juce::Time lastPerformanceUpdate;
    std::vector<float> frameTimes;
    int frameCounter = 0;
    
    //==============================================================================
    // Rendering Methods
    
    void updateAnimation(float deltaTime);
    void updateEffects(float deltaTime);
    void updatePerformanceMetrics();
    
    // Core rendering
    void renderSpectrum2D(juce::Graphics& g, juce::Rectangle<int> bounds);
    void renderSpectrum3D(juce::Graphics& g, juce::Rectangle<int> bounds);
    void renderWaveform3D(juce::Graphics& g, juce::Rectangle<int> bounds);
    void renderFrequencyBars(juce::Graphics& g, juce::Rectangle<int> bounds);
    void renderSpectrumSphere(juce::Graphics& g, juce::Rectangle<int> bounds);
    void renderParticleField(juce::Graphics& g, juce::Rectangle<int> bounds);
    
    // Effect rendering
    void renderPaintTrails(juce::Graphics& g);
    void renderParticles(juce::Graphics& g);
    void renderScreenEffects(juce::Graphics& g, juce::Rectangle<int> bounds);
    void renderGrid(juce::Graphics& g, juce::Rectangle<int> bounds);
    
    // Utility methods
    juce::Colour getSpectrumColor(float frequency, float magnitude) const;
    juce::AffineTransform get3DTransform(juce::Rectangle<int> bounds) const;
    void applyQualitySettings();
    void initializeDefaultColorSchemes();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VisualFeedbackEngine)
};