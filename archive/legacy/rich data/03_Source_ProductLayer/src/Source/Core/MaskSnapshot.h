#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <memory>

/**
 * MaskSnapshot - RT-safe mask data transfer system
 *
 * Implements atomic pointer swapping for lock-free communication between
 * GUI thread (painting) and audio thread (synthesis). The GUI paints to 
 * a work buffer, then atomically swaps it with the audio thread's snapshot.
 *
 * Key Features:
 * - Zero allocations in audio thread
 * - Atomic pointer swap at block boundary only
 * - Bilinear sampling for smooth interpolation
 * - Immutable snapshots prevent data races
 * - Sub-5ms performance impact
 */
class MaskSnapshot
{
public:
    //==============================================================================
    // Constants
    
    static constexpr int MASK_WIDTH = 512;    // Time resolution
    static constexpr int MASK_HEIGHT = 256;   // Frequency resolution  
    static constexpr int MASK_SIZE = MASK_WIDTH * MASK_HEIGHT;
    
    //==============================================================================
    // Mask Data Structure
    
    struct MaskData
    {
        // Mask values: 0.0 = fully attenuated, 1.0 = unaffected
        alignas(32) float maskValues[MASK_SIZE];     // 32-byte aligned for SIMD
        
        // Metadata for bilinear sampling
        float timeScale = 1.0f;      // Time axis scaling factor
        float freqScale = 1.0f;      // Frequency axis scaling factor
        float minFreq = 20.0f;       // Minimum frequency in Hz
        float maxFreq = 20000.0f;    // Maximum frequency in Hz
        
        // Feathering parameters
        float featherTime = 0.01f;   // Time feathering in seconds
        float featherFreq = 100.0f;  // Frequency feathering in Hz
        float threshold = -30.0f;    // Threshold for mask application in dB
        bool protectHarmonics = true; // Protect harmonic content
        
        // Generation timestamp for debugging
        juce::uint64 timestamp = 0;
        
        MaskData()
        {
            // Initialize with no masking (all 1.0)
            std::fill(std::begin(maskValues), std::end(maskValues), 1.0f);
            timestamp = juce::Time::getMillisecondCounterHiRes();
        }
        
        // Get mask value with bounds checking
        inline float getMaskValue(int x, int y) const noexcept
        {
            if (x < 0 || x >= MASK_WIDTH || y < 0 || y >= MASK_HEIGHT)
                return 1.0f;
            return maskValues[y * MASK_WIDTH + x];
        }
        
        // Set mask value with bounds checking
        inline void setMaskValue(int x, int y, float value) noexcept
        {
            if (x >= 0 && x < MASK_WIDTH && y >= 0 && y < MASK_HEIGHT)
                maskValues[y * MASK_WIDTH + x] = juce::jlimit(0.0f, 1.0f, value);
        }
        
        // Bilinear sampling for smooth interpolation
        inline float sampleBilinear(float x, float y) const noexcept
        {
            // Clamp to valid range
            x = juce::jlimit(0.0f, float(MASK_WIDTH - 1), x);
            y = juce::jlimit(0.0f, float(MASK_HEIGHT - 1), y);
            
            int x0 = int(x);
            int y0 = int(y);
            int x1 = juce::jmin(x0 + 1, MASK_WIDTH - 1);
            int y1 = juce::jmin(y0 + 1, MASK_HEIGHT - 1);
            
            float fx = x - float(x0);
            float fy = y - float(y0);
            
            float v00 = getMaskValue(x0, y0);
            float v10 = getMaskValue(x1, y0);
            float v01 = getMaskValue(x0, y1);
            float v11 = getMaskValue(x1, y1);
            
            float v0 = v00 + fx * (v10 - v00);
            float v1 = v01 + fx * (v11 - v01);
            
            return v0 + fy * (v1 - v0);
        }
    };
    
    //==============================================================================
    // Main Interface
    
    MaskSnapshot();
    ~MaskSnapshot();
    
    // Audio thread interface (RT-safe)
    void prepareToPlay(double sampleRate, int samplesPerBlock);
    
    // Get current snapshot for audio processing (RT-safe, returns immutable pointer)
    const MaskData* getCurrentSnapshot() const noexcept
    {
        return currentSnapshot.load(std::memory_order_acquire);
    }
    
    // Sample mask at given time and frequency (RT-safe bilinear interpolation)
    float sampleMask(float timeNorm, float frequencyHz, double sampleRate) const noexcept;
    
    // RT-SAFE: Fast math approximations
    static inline float fastLog2(float x) noexcept {
        union { float f; uint32_t i; } vx = { x };
        float y = (float)(vx.i) * 1.1920928955078125e-7f;
        return y - 126.94269504f;
    }
    
    // GUI thread interface (not RT-safe, but lock-free)
    
    // Get work buffer for GUI painting
    MaskData* getWorkBuffer() noexcept { return workBuffer.get(); }
    
    // Commit work buffer to audio thread (atomic swap)
    void commitWorkBuffer() noexcept;
    
    // Clear work buffer
    void clearWorkBuffer() noexcept;
    
    // Convenience methods for painting
    void paintCircle(float centerX, float centerY, float radius, float value);
    void paintRectangle(float x, float y, float width, float height, float value);
    void paintLine(float x1, float y1, float x2, float y2, float lineWidth, float value);
    
    // Parameter control (thread-safe)
    void setMaskBlend(float blend) noexcept { maskBlend.store(blend, std::memory_order_release); }
    void setMaskStrength(float strength) noexcept { maskStrength.store(strength, std::memory_order_release); }
    void setFeatherTime(float time) noexcept { featherTime.store(time, std::memory_order_release); }
    void setFeatherFreq(float freq) noexcept { featherFreq.store(freq, std::memory_order_release); }
    void setThreshold(float threshold) noexcept { this->threshold.store(threshold, std::memory_order_release); }
    void setProtectHarmonics(bool protect) noexcept { protectHarmonics.store(protect, std::memory_order_release); }
    
    float getMaskBlend() const noexcept { return maskBlend.load(std::memory_order_acquire); }
    float getMaskStrength() const noexcept { return maskStrength.load(std::memory_order_acquire); }
    float getFeatherTime() const noexcept { return featherTime.load(std::memory_order_acquire); }
    float getFeatherFreq() const noexcept { return featherFreq.load(std::memory_order_acquire); }
    float getThreshold() const noexcept { return threshold.load(std::memory_order_acquire); }
    bool getProtectHarmonics() const noexcept { return protectHarmonics.load(std::memory_order_acquire); }
    
    // Statistics for performance monitoring
    struct Statistics
    {
        juce::uint64 swapCount = 0;
        juce::uint64 lastSwapTime = 0;
        float averageCpuPercent = 0.0f;
        int activeMaskPixels = 0;  // Number of pixels != 1.0
    };
    
    Statistics getStatistics() const noexcept { return statistics; }

private:
    //==============================================================================
    // Internal Implementation
    
    // Triple buffer system for lock-free operation
    std::unique_ptr<MaskData> workBuffer;      // GUI paints here
    std::unique_ptr<MaskData> pendingBuffer;   // Ready to swap
    std::unique_ptr<MaskData> audioBuffer1;    // Audio thread snapshot 1  
    std::unique_ptr<MaskData> audioBuffer2;    // Audio thread snapshot 2
    
    // Atomic pointer to current snapshot (audio thread reads this)
    std::atomic<const MaskData*> currentSnapshot{nullptr};
    
    // Parameters (thread-safe atomics)
    std::atomic<float> maskBlend{1.0f};        // 0.0 = no mask, 1.0 = full mask
    std::atomic<float> maskStrength{1.0f};     // Mask intensity multiplier
    std::atomic<float> featherTime{0.01f};     // Time feathering in seconds
    std::atomic<float> featherFreq{100.0f};    // Frequency feathering in Hz
    std::atomic<float> threshold{-30.0f};      // Threshold in dB
    std::atomic<bool> protectHarmonics{true};  // Protect harmonic content
    
    // Audio processing state
    double currentSampleRate = 44100.0;
    int samplesPerBlock = 512;
    
    // Statistics
    mutable Statistics statistics;
    mutable juce::uint64 lastCpuMeasureTime = 0;
    
    // Helper methods
    inline float frequencyToY(float frequencyHz, const MaskData* snapshot) const noexcept;
    inline float timeToX(float timeNorm) const noexcept;
    void updateStatistics() const noexcept;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MaskSnapshot)
};