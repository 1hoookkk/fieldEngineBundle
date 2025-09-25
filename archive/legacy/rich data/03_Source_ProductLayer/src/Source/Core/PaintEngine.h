#pragma once

#include <JuceHeader.h>
#include <vector>
#include <memory>
#include <atomic>
#include <unordered_map>

/**
 * Real-time audio painting engine for SoundCanvas
 * Converts brush strokes and canvas interactions into live audio synthesis
 * 
 * Core Philosophy:
 * - Sub-10ms latency from stroke to sound
 * - Support for multiple synthesis engines
 * - Infinite canvas with efficient sparse storage
 * - MetaSynth-inspired X=time, Y=pitch mapping
 */
class PaintEngine
{
public:
    //==============================================================================
    // Core Types
    
    struct Point
    {
        float x = 0.0f;        // Canvas X coordinate (time domain)
        float y = 0.0f;        // Canvas Y coordinate (frequency domain)
        
        Point() = default;
        Point(float x_, float y_) : x(x_), y(y_) {}
        
        bool operator==(const Point& other) const noexcept
        {
            return juce::approximatelyEqual(x, other.x) && 
                   juce::approximatelyEqual(y, other.y);
        }
    };
    
    struct AudioParams
    {
        float frequency = 440.0f;     // Hz - derived from Y position
        float amplitude = 0.0f;       // 0.0-1.0 - derived from brush pressure/brightness
        float pan = 0.5f;             // 0.0=left, 0.5=center, 1.0=right
        float time = 0.0f;            // Temporal position in canvas
        
        // Extended parameters for advanced synthesis
        float filterCutoff = 1.0f;    // 0.0-1.0
        float resonance = 0.0f;       // 0.0-1.0
        float modDepth = 0.0f;        // 0.0-1.0
        
        AudioParams() = default;
        AudioParams(float freq, float amp, float p, float t)
            : frequency(freq), amplitude(amp), pan(p), time(t) {}
    };
    
    struct StrokePoint
    {
        Point position;
        float pressure = 1.0f;        // 0.0-1.0 from input device
        float velocity = 0.0f;        // Derived from stroke speed
        juce::Colour color;           // RGBA color information
        juce::uint32 timestamp;       // When this point was created
        
        StrokePoint() = default;
        StrokePoint(Point pos, float press, juce::Colour col)
            : position(pos), pressure(press), color(col), 
              timestamp(juce::Time::getMillisecondCounter()) {}
    };
    
    // Forward declaration
    class SynthEngine;
    
    //==============================================================================
    // Main Interface
    
    PaintEngine();
    ~PaintEngine();
    
    // Audio processing lifecycle
    void prepareToPlay(double sampleRate, int samplesPerBlock);
    void processBlock(juce::AudioBuffer<float>& buffer);
    void releaseResources();
    
    // Stroke interaction API
    void beginStroke(Point position, float pressure = 1.0f, juce::Colour color = juce::Colours::white);
    void updateStroke(Point position, float pressure = 1.0f);
    void endStroke();
    
    // Canvas control
    void setPlayheadPosition(float normalisedPosition);
    void setCanvasRegion(float leftX, float rightX, float bottomY, float topY);
    void clearCanvas();
    void clearRegion(const juce::Rectangle<float>& region);
    
    // Audio parameters
    void setActive(bool shouldBeActive) { isActive.store(shouldBeActive); }
    bool getActive() const { return isActive.load(); }
    bool prepared() const noexcept { return isPrepared.load(std::memory_order_acquire); }
    void setMasterGain(float gain);
    void setFrequencyRange(float minHz, float maxHz);
    void setUsePanning(bool shouldUsePanning) { usePanning.store(shouldUsePanning); }
    
    // Canvas mapping functions
    float canvasYToFrequency(float y) const;
    float frequencyToCanvasY(float frequency) const;
    float canvasXToTime(float x) const;
    float timeToCanvasX(float time) const;
    
    // Performance monitoring
    float getCurrentCPULoad() const { return cpuLoad.load(); }
    int getActiveOscillatorCount() const { return activeOscillators.load(); }
    
private:
    //==============================================================================
    // Internal Classes
    
    /**
     * Drift-free complex phasor oscillator
     * Uses z[n+1] = z[n] * e^(j*omega) rotation for exact phase accuracy
     */
    struct Phasor
    {
        std::complex<float> z{1.0f, 0.0f};  // Complex phasor: z = e^(j*omega*t)
        std::complex<float> rotation{1.0f, 0.0f};  // Precomputed rotation: e^(j*omega)
        uint32_t sampleCount{0};  // Counter for periodic renormalization
        static constexpr uint32_t RENORM_INTERVAL = 256;  // Renormalize every 256 samples
        
        void setFrequency(float frequency, float sampleRate)
        {
            // Clamp frequency to sampleRate/3 for Nyquist safety
            const float maxFreq = sampleRate / 3.0f;
            const float clampedFreq = juce::jlimit(1.0f, maxFreq, frequency);
            
            // Compute rotation per sample: e^(j*2π*f/fs)
            const float omega = juce::MathConstants<float>::twoPi * clampedFreq / sampleRate;
            rotation = std::complex<float>(std::cos(omega), std::sin(omega));
        }
        
        void reset(float initialPhase = 0.0f)
        {
            z = std::complex<float>(std::cos(initialPhase), std::sin(initialPhase));
            sampleCount = 0;
        }
        
        std::complex<float> step()
        {
            // Complex rotation: z[n+1] = z[n] * e^(j*omega)
            z *= rotation;
            
            // Periodic renormalization to prevent numerical drift
            if (++sampleCount >= RENORM_INTERVAL)
            {
                float magnitude = std::abs(z);
                if (magnitude > 1e-6f)  // Avoid division by zero
                {
                    z /= magnitude;  // Renormalize to unit circle
                }
                sampleCount = 0;
            }
            
            return z;
        }
        
        float getSine() const
        {
            return z.imag();  // Imaginary part = sin(phase)
        }
        
        float getCosine() const
        {
            return z.real();  // Real part = cos(phase)
        }
    };

    /**
     * Represents a single oscillator/partial in the synthesis
     */
    class Oscillator
    {
    public:
        Oscillator() = default;
        
        void setParameters(const AudioParams& params);
        void updatePhase(float sampleRate);
        float getSample() const;
        bool isActive() const { return getAmplitude() > 0.0001f || getTargetAmplitude() > 0.0001f; }
        
        // Smooth parameter changes to prevent clicks
        void smoothParameters(float smoothingFactor = 0.05f);
        
        // Reset oscillator to default state
        void reset() 
        {
            frequency = 440.0f;
            amplitude = 0.0f;
            targetAmplitude = 0.0f;
            phase = 0.0f;
            pan = 0.5f;
            targetPan = 0.5f;
            phaseIncrement = 0.0f;
            phasor.reset();
        }
        
        // Public accessors for private members
        float getFrequency() const { return frequency; }
        void setFrequency(float freq) { frequency = freq; }
        
        float getAmplitude() const { return amplitude; }
        void setAmplitude(float amp) { amplitude = amp; }
        
        float getTargetAmplitude() const { return targetAmplitude; }
        void setTargetAmplitude(float amp) { targetAmplitude = amp; }
        
        float getPhase() const { return phase; }
        void setPhase(float ph) { phase = ph; }
        
        float getPan() const { return pan; }
        void setPan(float p) { pan = p; }
        
        float getTargetPan() const { return targetPan; }
        void setTargetPan(float p) { targetPan = p; }
        
        float getPhaseIncrement() const { return phaseIncrement; }
        void setPhaseIncrement(float inc) { phaseIncrement = inc; }
        
        // Phasor access for initialization
        void resetPhasor(float initialPhase = 0.0f) { phasor.reset(initialPhase); }
        void setPhasorFrequency(float freq, float sampleRate) { phasor.setFrequency(freq, sampleRate); }
        
    private:
        float frequency = 440.0f;
        float amplitude = 0.0f;
        float targetAmplitude = 0.0f;
        float phase = 0.0f;
        float pan = 0.5f;
        float targetPan = 0.5f;
        
        // Phase increment is cached for performance
        float phaseIncrement = 0.0f;
        
        // Drift-free phasor oscillator
        Phasor phasor;
    };
    
    /**
     * Represents a painted stroke on the canvas
     */
    class Stroke
    {
    public:
        Stroke(juce::uint32 id);
        
        void addPoint(const StrokePoint& point);
        void finalize();
        
        bool isActive() const { return !isFinalized || hasActiveOscillators(); }
        void updateOscillators(float currentTime, std::vector<Oscillator>& oscillatorPool);
        
        const std::vector<StrokePoint>& getPoints() const { return points; }
        juce::uint32 getId() const { return strokeId; }
        
    private:
        juce::uint32 strokeId;
        std::vector<StrokePoint> points;
        bool isFinalized = false;
        
        // Cached bounds for optimization
        juce::Rectangle<float> bounds;
        void updateBounds();
        
        bool hasActiveOscillators() const;
        
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Stroke)
    };
    
    /**
     * Sparse storage for canvas regions
     */
    class CanvasRegion
    {
    public:
        static constexpr int REGION_SIZE = 64;  // 64x64 pixel regions
        
        CanvasRegion(int regionX, int regionY);
        
        void addStroke(std::shared_ptr<Stroke> stroke);
        void removeStroke(juce::uint32 strokeId);
        void updateOscillators(float currentTime, std::vector<Oscillator>& oscillatorPool);
        
        bool isEmpty() const { return strokes.empty(); }
        int getRegionX() const { return regionX; }
        int getRegionY() const { return regionY; }
        
    private:
        int regionX, regionY;
        std::vector<std::shared_ptr<Stroke>> strokes;
        
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CanvasRegion)
    };
    
    //==============================================================================
    // Member Variables
    
    // Audio processing state
    std::atomic<bool> isActive{ false };
    std::atomic<bool> isPrepared{ false };  // SAFETY: Track initialization state
    std::atomic<bool> usePanning{ true };
    std::atomic<float> cpuLoad{ 0.0f };
    std::atomic<int> activeOscillators{ 0 };
    
    double sampleRate = 44100.0;
    int samplesPerBlock = 512;
    
    // Canvas state
    float playheadPosition = 0.0f;      // 0.0-1.0 normalized position
    float canvasLeft = -100.0f;         // Canvas bounds in arbitrary units
    float canvasRight = 100.0f;
    float canvasBottom = -50.0f;
    float canvasTop = 50.0f;
    
    // Frequency mapping
    float minFrequency = 20.0f;
    float maxFrequency = 20000.0f;
    bool useLogFrequencyScale = true;
    
    // RELIABILITY FIX: Lock-free double-buffered oscillator pool for performance
    static constexpr int MAX_OSCILLATORS = 1024;
    
    // Double-buffered oscillator pools (front/back buffer)
    std::array<std::vector<Oscillator>, 2> oscillatorPools;
    std::atomic<int> frontBufferIndex{ 0 };  // Index of buffer being read by audio thread
    std::atomic<int> backBufferIndex{ 1 };   // Index of buffer being written by GUI thread
    std::atomic<bool> bufferSwapPending{ false };  // Signal for buffer swap
    
    // Stroke management
    std::unique_ptr<Stroke> currentStroke;
    juce::uint32 nextStrokeId = 1;
    
    // Sparse canvas storage
    std::unordered_map<juce::int64, std::unique_ptr<CanvasRegion>> canvasRegions;
    
    // Audio processing
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> masterGain;
    
    // Performance monitoring
    juce::uint32 lastProcessTime = 0;
    
    // RELIABILITY FIX: Removed oscillatorLock mutex to prevent audio thread starvation
    
    //==============================================================================
    // Private Methods
    
    void updateCanvasOscillators();
    juce::int64 getRegionKey(int regionX, int regionY) const;
    CanvasRegion* getOrCreateRegion(float canvasX, float canvasY);
    void cullInactiveRegions();
    
    // Audio parameter conversion with enhanced musical responsiveness
    AudioParams strokePointToAudioParams(const StrokePoint& point) const;
    AudioParams strokePointToAudioParamsMusical(const StrokePoint& point, float velocity = 0.0f) const;
    
    // Performance optimization
    void updateCPULoad();
    void optimizeOscillatorPool();
    void createDefaultTestOscillator();  // AUDIO FIX: Generate test tone
    
    // RELIABILITY FIX: Lock-free buffer management methods
    std::vector<Oscillator>& getFrontBuffer() { return oscillatorPools[frontBufferIndex.load()]; }
    const std::vector<Oscillator>& getFrontBuffer() const { return oscillatorPools[frontBufferIndex.load()]; }
    std::vector<Oscillator>& getBackBuffer() { return oscillatorPools[backBufferIndex.load()]; }
    void swapBuffersIfPending();  // Called from audio thread only
    void requestBufferSwap();     // Called from GUI thread to request swap
    
    // PHASE 1 OPTIMIZATIONS: Sub-10ms Latency Paint-to-Audio Pipeline
    
    // Spatial partitioning for efficient oscillator lookup
    static constexpr int GRID_SIZE = 32;  // 32x32 grid for spatial partitioning
    static constexpr float INFLUENCE_RADIUS = 5.0f;  // Radius of influence for stroke points
    
    struct SpatialGrid {
        std::vector<std::vector<int>> oscillatorIndices;  // Grid cells containing oscillator indices
        float cellWidth, cellHeight;
        
        SpatialGrid() : oscillatorIndices(GRID_SIZE * GRID_SIZE) {}
        
        void initialize(float canvasWidth, float canvasHeight) {
            cellWidth = canvasWidth / GRID_SIZE;
            cellHeight = canvasHeight / GRID_SIZE;
            clearGrid();
        }
        
        void clearGrid() {
            for (auto& cell : oscillatorIndices) {
                cell.clear();
            }
        }
        
        int getCellIndex(float x, float y, float canvasLeft, float canvasBottom) const {
            int gridX = static_cast<int>((x - canvasLeft) / cellWidth);
            int gridY = static_cast<int>((y - canvasBottom) / cellHeight);
            gridX = juce::jlimit(0, GRID_SIZE - 1, gridX);
            gridY = juce::jlimit(0, GRID_SIZE - 1, gridY);
            return gridY * GRID_SIZE + gridX;
        }
        
        std::vector<int> getNearbyOscillators(float x, float y, float canvasLeft, float canvasBottom) const {
            std::vector<int> result;
            int centerCell = getCellIndex(x, y, canvasLeft, canvasBottom);
            
            // Check center cell and 8 surrounding cells for comprehensive coverage
            int centerX = centerCell % GRID_SIZE;
            int centerY = centerCell / GRID_SIZE;
            
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    int nx = centerX + dx;
                    int ny = centerY + dy;
                    
                    if (nx >= 0 && nx < GRID_SIZE && ny >= 0 && ny < GRID_SIZE) {
                        int cellIndex = ny * GRID_SIZE + nx;
                        const auto& cell = oscillatorIndices[cellIndex];
                        result.insert(result.end(), cell.begin(), cell.end());
                    }
                }
            }
            
            return result;
        }
    };
    
    SpatialGrid spatialGrid;
    
    // Incremental oscillator updates (only affect nearby oscillators)
    void updateOscillatorsIncremental(const StrokePoint& newPoint);
    void assignOscillatorToGrid(int oscillatorIndex, float x, float y);
    void rebuildSpatialGrid();
    
    // Enhanced oscillator with sophisticated envelope and parameter smoothing
    struct EnhancedOscillatorState {
        bool inUse = false;
        float lastUsedTime = 0.0f;
        
        // Envelope state for smooth activation/deactivation
        enum class EnvelopePhase { Inactive, Attack, Sustain, Release };
        EnvelopePhase envelopePhase = EnvelopePhase::Inactive;
        float envelopeValue = 0.0f;
        
        // Dynamic envelope timing based on velocity and pressure
        float baseAttackRate = 0.05f;   // Fast attack for immediate response (50ms)
        float baseReleaseRate = 0.3f;   // Smooth release for natural decay (300ms)
        float velocityModulation = 1.0f; // Velocity affects attack/release speed
        
        // Parameter smoothing to prevent clicks
        float targetFrequency = 440.0f;
        float targetAmplitude = 0.0f;
        float targetPan = 0.5f;
        
        // Enhanced smoothing with velocity sensitivity
        static constexpr float FREQUENCY_SMOOTHING = 0.02f;  // Slower for pitch stability
        static constexpr float AMPLITUDE_SMOOTHING = 0.15f;  // Faster for dynamic response
        static constexpr float PAN_SMOOTHING = 0.08f;        // Moderate for spatial changes
        
        // Envelope curve shapes for more musical response
        enum class CurveType { Linear, Exponential, Logarithmic };
        CurveType attackCurve = CurveType::Exponential;   // Fast start, slow finish
        CurveType releaseCurve = CurveType::Logarithmic;  // Slow start, fast finish
        
        void updateEnvelope(float sampleRate) {
            // Dynamic rate calculation based on velocity
            const float actualAttackRate = baseAttackRate / velocityModulation;
            const float actualReleaseRate = baseReleaseRate * (0.5f + velocityModulation * 0.5f);
            
            const float attackIncrement = 1.0f / (actualAttackRate * sampleRate);
            const float releaseIncrement = 1.0f / (actualReleaseRate * sampleRate);
            
            switch (envelopePhase) {
                case EnvelopePhase::Attack:
                {
                    float rawValue = envelopeValue + attackIncrement;
                    
                    // Apply attack curve shape
                    switch (attackCurve) {
                        case CurveType::Exponential:
                            envelopeValue = 1.0f - std::exp(-rawValue * 4.0f);
                            break;
                        case CurveType::Logarithmic:
                            envelopeValue = std::log(1.0f + rawValue * 9.0f) / std::log(10.0f);
                            break;
                        case CurveType::Linear:
                        default:
                            envelopeValue = rawValue;
                            break;
                    }
                    
                    if (envelopeValue >= 1.0f) {
                        envelopeValue = 1.0f;
                        envelopePhase = EnvelopePhase::Sustain;
                    }
                    break;
                }
                    
                case EnvelopePhase::Release:
                {
                    float rawValue = envelopeValue - releaseIncrement;
                    
                    // Apply release curve shape
                    switch (releaseCurve) {
                        case CurveType::Exponential:
                            envelopeValue = rawValue * std::exp(-releaseIncrement * 2.0f);
                            break;
                        case CurveType::Logarithmic:
                            envelopeValue = rawValue * rawValue; // Quadratic decay
                            break;
                        case CurveType::Linear:
                        default:
                            envelopeValue = rawValue;
                            break;
                    }
                    
                    if (envelopeValue <= 0.0f) {
                        envelopeValue = 0.0f;
                        envelopePhase = EnvelopePhase::Inactive;
                        inUse = false;
                    }
                    break;
                }
                    
                case EnvelopePhase::Sustain:
                    // Add subtle amplitude modulation during sustain for organic feel
                    envelopeValue = 1.0f + std::sin(lastUsedTime * 0.001f) * 0.05f;
                    envelopeValue = juce::jlimit(0.95f, 1.0f, envelopeValue);
                    break;
                    
                case EnvelopePhase::Inactive:
                default:
                    break;
            }
        }
        
        void activate(float velocity = 1.0f) {
            if (!inUse) {
                inUse = true;
                envelopePhase = EnvelopePhase::Attack;
                envelopeValue = 0.0f;
                velocityModulation = juce::jlimit(0.1f, 2.0f, velocity);
                
                // Adjust curve types based on velocity
                if (velocity > 0.8f) {
                    attackCurve = CurveType::Linear;    // Fast, punchy attack
                    releaseCurve = CurveType::Exponential; // Quick decay
                } else {
                    attackCurve = CurveType::Exponential; // Smooth attack
                    releaseCurve = CurveType::Logarithmic; // Natural decay
                }
            }
        }
        
        void release() {
            if (inUse && envelopePhase != EnvelopePhase::Release) {
                envelopePhase = EnvelopePhase::Release;
            }
        }
        
        bool isActive() const {
            return inUse && envelopePhase != EnvelopePhase::Inactive;
        }
        
        // Get envelope-modulated parameter values for musical expression
        float getModulatedAmplitude() const {
            return targetAmplitude * envelopeValue;
        }
        
        float getModulatedFrequency() const {
            // Subtle pitch bend during attack for more organic sound
            float pitchMod = (envelopePhase == EnvelopePhase::Attack) ? 
                (1.0f - envelopeValue) * 0.02f : 0.0f;
            return targetFrequency * (1.0f + pitchMod);
        }
    };
    
    std::vector<EnhancedOscillatorState> oscillatorStates;
    
    // Optimized oscillator allocation with age-based replacement
    std::vector<int> freeOscillatorIndices;
    int findBestOscillatorForReplacement() const;
    void activateOscillator(int index, const AudioParams& params);
    void releaseOscillator(int index);
    
    // Oscillator allocation and influence calculations
    bool shouldAllocateNewOscillator(const StrokePoint& newPoint) const;
    int allocateOscillator();
    void updateOscillatorWithInfluence(int oscillatorIndex, const StrokePoint& newPoint, const AudioParams& params);
    float calculateDistance(int oscillatorIndex, const Point& position) const;
    float calculateInfluence(float distance, float pressure) const;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PaintEngine)
};