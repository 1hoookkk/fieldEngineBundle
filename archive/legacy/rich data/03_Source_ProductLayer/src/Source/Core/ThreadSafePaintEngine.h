#pragma once
#include "PaintEngine.h"
#include <atomic>
#include <memory>

/**
 * Thread-safe wrapper for PaintEngine
 * 
 * This class provides thread-safe access to PaintEngine functionality by using
 * atomic operations and double-buffering techniques to minimize lock contention
 * between the GUI and audio threads.
 * 
 * Key improvements:
 * - Lock-free stroke management using atomic pointers
 * - Double-buffered oscillator state updates
 * - Atomic oscillator pool management
 * - Minimal critical sections for real-time performance
 */
class ThreadSafePaintEngine : public PaintEngine
{
public:
    ThreadSafePaintEngine();
    ~ThreadSafePaintEngine() override;
    
    // Thread-safe stroke interaction API (called from GUI thread)
    void beginStroke(Point position, float pressure = 1.0f, juce::Colour color = juce::Colours::white) override;
    void updateStroke(Point position, float pressure = 1.0f) override;
    void endStroke() override;
    
    // Thread-safe canvas control (called from GUI thread)
    void clearCanvas() override;
    void clearRegion(const juce::Rectangle<float>& region) override;
    
    // Audio processing (called from audio thread)
    void processBlock(juce::AudioBuffer<float>& buffer) override;
    
protected:
    /**
     * Double-buffered stroke data for lock-free updates
     */
    struct StrokeBuffer
    {
        std::vector<StrokePoint> pendingPoints;
        std::atomic<bool> hasNewData{false};
        std::atomic<bool> shouldBeginNewStroke{false};
        std::atomic<bool> shouldEndStroke{false};
        Point strokeStartPosition;
        float strokeStartPressure = 1.0f;
        juce::Colour strokeColor = juce::Colours::white;
    };
    
    // Double-buffered stroke data
    std::array<std::unique_ptr<StrokeBuffer>, 2> strokeBuffers;
    std::atomic<int> activeStrokeBuffer{0};
    std::atomic<int> processingStrokeBuffer{1};
    
    /**
     * Thread-safe oscillator allocation
     */
    struct OscillatorAllocator
    {
        // Atomic free list using indices
        std::atomic<int> freeListHead{0};
        std::array<std::atomic<int>, MAX_OSCILLATORS> freeListNext;
        std::atomic<int> freeCount{MAX_OSCILLATORS};
        
        void initialize();
        int allocate();
        void release(int index);
    };
    
    OscillatorAllocator oscillatorAllocator;
    
    /**
     * Atomic oscillator state management
     */
    struct AtomicOscillatorState
    {
        std::atomic<bool> inUse{false};
        std::atomic<float> frequency{440.0f};
        std::atomic<float> amplitude{0.0f};
        std::atomic<float> pan{0.5f};
        std::atomic<uint32_t> lastUpdateTime{0};
        
        // Envelope parameters
        std::atomic<int> envelopePhase{0}; // 0=Inactive, 1=Attack, 2=Sustain, 3=Release
        std::atomic<float> envelopeValue{0.0f};
        
        // Explicitly delete copy operations since atomics cannot be copied
        AtomicOscillatorState(const AtomicOscillatorState&) = delete;
        AtomicOscillatorState& operator=(const AtomicOscillatorState&) = delete;
        
        // Allow default construction and move operations
        AtomicOscillatorState() = default;
        AtomicOscillatorState(AtomicOscillatorState&& other) noexcept
        {
            inUse.store(other.inUse.load());
            frequency.store(other.frequency.load());
            amplitude.store(other.amplitude.load());
            pan.store(other.pan.load());
            lastUpdateTime.store(other.lastUpdateTime.load());
            envelopePhase.store(other.envelopePhase.load());
            envelopeValue.store(other.envelopeValue.load());
        }
        
        AtomicOscillatorState& operator=(AtomicOscillatorState&& other) noexcept
        {
            if (this != &other)
            {
                inUse.store(other.inUse.load());
                frequency.store(other.frequency.load());
                amplitude.store(other.amplitude.load());
                pan.store(other.pan.load());
                lastUpdateTime.store(other.lastUpdateTime.load());
                envelopePhase.store(other.envelopePhase.load());
                envelopeValue.store(other.envelopeValue.load());
            }
            return *this;
        }
    };
    
    std::array<AtomicOscillatorState, MAX_OSCILLATORS> atomicOscillatorStates;
    
    /**
     * Command queue for deferred operations
     */
    enum class DeferredCommand
    {
        ClearCanvas,
        ClearRegion,
        RebuildSpatialGrid
    };
    
    struct DeferredOperation
    {
        DeferredCommand command;
        juce::Rectangle<float> region;
    };
    
    // Lock-free queue for deferred operations
    static constexpr size_t MAX_DEFERRED_OPS = 64;
    std::array<DeferredOperation, MAX_DEFERRED_OPS> deferredOps;
    std::atomic<size_t> deferredOpsWrite{0};
    std::atomic<size_t> deferredOpsRead{0};
    
    // Helper methods
    void processStrokeBuffer(StrokeBuffer& buffer);
    void processDeferredOperations();
    void swapStrokeBuffers();
    bool pushDeferredOperation(const DeferredOperation& op);
    
    // Override protected methods for thread safety
    void updateCanvasOscillators() override;
    int allocateOscillator() override;
    void releaseOscillator(int index) override;
    void activateOscillator(int index, const AudioParams& params) override;
    
    // Performance monitoring
    std::atomic<double> lastStrokeProcessTime{0.0};
    std::atomic<int> droppedStrokes{0};
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ThreadSafePaintEngine)
};