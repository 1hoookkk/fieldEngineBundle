#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include "nodes/SpectralPaintNode.h"

/**
 * @file SpectralPaintProcessor.h
 * @brief JUCE AudioProcessor wrapper for SpectralPaintNode
 * 
 * Wraps the modern JUCE DSP node for integration with existing
 * SpectralCanvasPro architecture. Provides hybrid routing to allow
 * A/B testing between old SpectralEngine and new SpectralPaintNode.
 * 
 * Features:
 * - Zero-allocation RT processing via SpectralPaintNode
 * - Parameter atomics for thread-safe UI updates
 * - Queue statistics for performance monitoring
 * - Hybrid mode for gradual migration testing
 */

class SpectralPaintProcessor
{
public:
    /// Initialize processor
    SpectralPaintProcessor();
    
    /// Virtual destructor for inheritance
    virtual ~SpectralPaintProcessor() = default;
    
    // Non-copyable for safety
    SpectralPaintProcessor(const SpectralPaintProcessor&) = delete;
    SpectralPaintProcessor& operator=(const SpectralPaintProcessor&) = delete;
    
    /**
     * @brief Initialize with audio specifications
     * @param spec JUCE ProcessSpec with sample rate and buffer size
     */
    void prepare(const juce::dsp::ProcessSpec& spec);
    
    /**
     * @brief Reset processing state
     * Called when playback stops or sample rate changes.
     */
    void reset() noexcept;
    
    /**
     * @brief Process audio block (RT-safe)
     * @param context JUCE processing context with input/output buffers
     * 
     * Thread Safety: Audio thread only, never blocks or allocates
     */
    void processBlock(const juce::dsp::ProcessContextReplacing<float>& context) noexcept;
    
    /**
     * @brief Push paint event from UI thread
     * @param y Normalized Y coordinate [0,1] for log-freq mapping
     * @param intensity Brush intensity [0,1]
     * @param timestampMs Event timestamp in milliseconds
     * @return true if queued, false if dropped due to full queue
     * 
     * Thread Safety: UI thread only, never blocks
     */
    bool pushPaintEvent(float y, float intensity, uint32_t timestampMs) noexcept;
    
    /**
     * @brief Enable/disable pink noise bed
     * @param enabled If true, adds -36dBFS pink noise for audible strokes
     */
    void setPinkBedEnabled(bool enabled) noexcept;
    
    /**
     * @brief Set brush radius for radial boosting
     * @param radius Radius in FFT bins (1-32 range)
     */
    void setBrushRadius(int radius) noexcept;
    
    /**
     * @brief Enable/disable processing bypass
     * @param bypassed If true, passes input directly to output
     */
    void setBypassed(bool bypassed) noexcept { bypassed_.store(bypassed, std::memory_order_relaxed); }
    
    /// Get current configuration
    int getFFTOrder() const noexcept;
    size_t getFFTSize() const noexcept;
    int getHopSize() const noexcept;
    int getNumBins() const noexcept;
    
    /// Queue statistics for performance monitoring
    size_t getMaskQueueDepth() const noexcept;
    size_t getMaskDropCount() const noexcept;
    
    /// Check if processor is initialized
    bool isInitialized() const noexcept { return initialized_.load(std::memory_order_acquire); }

private:
    /// Core DSP node
    std::unique_ptr<nodes::SpectralPaintNode> spectralNode_;
    
    /// State management
    std::atomic<bool> initialized_{false};
    std::atomic<bool> bypassed_{false};
    
    /// Audio parameters
    double sampleRate_ = 48000.0;
    int maxBlockSize_ = 512;
    
    /**
     * @brief Validate parameters before RT processing
     * @param y Y coordinate to validate
     * @param intensity Intensity to validate
     * @return true if parameters are valid for processing
     */
    bool validateParameters(float y, float intensity) const noexcept;
};