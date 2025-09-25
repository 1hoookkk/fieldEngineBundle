#pragma once

#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <memory>
#include <array>
#include "RealtimeSafeTypes.h"
#include "AtlasIds.h"

/**
 * ReconfigManager - Safe FFT/hop/latency reconfiguration system
 * 
 * Handles reconfiguration of FFT parameters without allocating on the audio thread.
 * Uses double-buffered plans that can be swapped atomically at safe gate points.
 * 
 * Key principle: Never "live-flip" FFT parameters mid-callback - always pre-plan
 * and swap at designated gates (prepareToPlay or setProcessing transitions).
 */
class ReconfigManager
{
public:
    // Supported FFT sizes (must be powers of 2)
    enum class FFTSize : uint16_t
    {
        Size256 = 256,
        Size512 = 512,
        Size1024 = 1024,
        Size2048 = 2048
    };
    
    // Audio processing configuration
    struct EngineConfig
    {
        double sampleRate = 44100.0;
        int maxBlockSize = 512;
        FFTSize fftSize = FFTSize::Size512;
        int hopSize = 128;                    // FFT_SIZE/4 typically
        int numChannels = 2;
        bool enableOverlap = true;            // Overlap-add processing
        
        // Derived properties
        int getFFTSizeInt() const noexcept { return static_cast<int>(fftSize); }
        int getNumBins() const noexcept { return getFFTSizeInt() / 2 + 1; }
        int getOverlapBufferSize() const noexcept { return getFFTSizeInt() * 4; }
    };
    
    // Pre-allocated FFT plan with buffers
    struct FFTPlan
    {
        std::unique_ptr<juce::dsp::FFT> forwardFFT;
        std::unique_ptr<juce::dsp::FFT> inverseFFT;
        RTWindow<2048> hannWindow;            // Max window size
        
        // Pre-allocated buffers (sized for largest FFT)
        AlignedStorage<std::array<float, 2048>> fftBuffer;
        AlignedStorage<std::array<float, 2048>> ifftBuffer;
        AlignedStorage<std::array<float, 2048>> windowedBuffer;
        AlignedStorage<std::array<float, 8192>> overlapBuffer; // FFT_SIZE * 4
        
        EngineConfig config;
        bool initialized = false;
        
        FFTPlan() = default;
        
        // Initialize plan with specific configuration (may allocate)
        bool initialize(const EngineConfig& cfg) noexcept
        {
            try
            {
                config = cfg;
                
                // Create FFT objects (this can allocate)
                const int fftSizeInt = cfg.getFFTSizeInt();
                const int fftOrder = static_cast<int>(std::log2(fftSizeInt));
                
                forwardFFT = std::make_unique<juce::dsp::FFT>(fftOrder);
                inverseFFT = std::make_unique<juce::dsp::FFT>(fftOrder);
                
                // Initialize Hann window  
                // hannWindow.initialize(fftSizeInt);  // RTWindow doesn't have initialize method
                
                // Clear all buffers
                std::fill(fftBuffer.data->begin(), fftBuffer.data->end(), 0.0f);
                std::fill(ifftBuffer.data->begin(), ifftBuffer.data->end(), 0.0f);
                std::fill(windowedBuffer.data->begin(), windowedBuffer.data->end(), 0.0f);
                std::fill(overlapBuffer.data->begin(), overlapBuffer.data->end(), 0.0f);
                
                initialized = true;
                return true;
            }
            catch (...)
            {
                initialized = false;
                return false;
            }
        }
        
        void reset() noexcept
        {
            if (!initialized) return;
            
            // Clear overlap buffers but preserve FFT objects
            std::fill(overlapBuffer.data->begin(), overlapBuffer.data->end(), 0.0f);
        }
    };
    
    ReconfigManager() noexcept = default;
    ~ReconfigManager() = default;
    
    // Non-copyable for thread safety
    ReconfigManager(const ReconfigManager&) = delete;
    ReconfigManager& operator=(const ReconfigManager&) = delete;
    
    // Initialize with default configuration (called from constructor - may allocate)
    bool initialize(const EngineConfig& defaultConfig)
    {
        currentConfig_ = defaultConfig;
        pendingConfig_ = defaultConfig;
        
        // Initialize both plans with default config
        if (!activePlan_.initialize(defaultConfig)) return false;
        if (!standbyPlan_.initialize(defaultConfig)) return false;
        
        reconfigInProgress_.store(false, std::memory_order_release);
        return true;
    }
    
    // Request configuration change (called from UI thread)
    bool requestReconfig(const EngineConfig& newConfig) noexcept
    {
        // Don't queue duplicate reconfigs
        if (memcmp(&newConfig, &pendingConfig_, sizeof(EngineConfig)) == 0)
            return true;
        
        pendingConfig_ = newConfig;
        reconfigRequested_.store(true, std::memory_order_release);
        return true;
    }
    
    // Check if reconfiguration is needed and prepare standby plan (called from audio thread gates)
    bool prepareReconfigIfNeeded()
    {
        if (!reconfigRequested_.load(std::memory_order_acquire))
            return false;
        
        // Initialize standby plan with pending config (this may allocate - only call at gates!)
        if (!standbyPlan_.initialize(pendingConfig_))
            return false;
        
        reconfigInProgress_.store(true, std::memory_order_release);
        return true;
    }
    
    // Apply prepared reconfiguration atomically (called from audio thread at gate)
    void applyReconfig() noexcept
    {
        if (!reconfigInProgress_.load(std::memory_order_acquire))
            return;
        
        // Atomic swap of plans
        std::swap(activePlan_, standbyPlan_);
        currentConfig_ = activePlan_.config;
        
        // Clear flags
        reconfigRequested_.store(false, std::memory_order_release);
        reconfigInProgress_.store(false, std::memory_order_release);
    }
    
    // Get current configuration (RT-safe)
    const EngineConfig& getCurrentConfig() const noexcept { return currentConfig_; }
    
    // Get active FFT plan (RT-safe)
    const FFTPlan& getActivePlan() const noexcept { return activePlan_; }
    FFTPlan& getActivePlan() noexcept { return activePlan_; }
    
    // Check if reconfiguration is available to apply
    bool isReconfigReady() const noexcept { return reconfigInProgress_.load(std::memory_order_acquire); }
    bool isReconfigRequested() const noexcept { return reconfigRequested_.load(std::memory_order_acquire); }
    
    // Reset current plan state (RT-safe)
    void resetCurrentPlan() noexcept
    {
        activePlan_.reset();
    }
    
    // VST3 setProcessing integration
    void handleSetProcessing(bool processing)
    {
        if (!processing)
        {
            // Processing stopped - safe gate for reconfiguration
            if (prepareReconfigIfNeeded())
            {
                // Reconfiguration prepared successfully
            }
        }
        else
        {
            // Processing starting - apply any prepared reconfigurations
            applyReconfig();
            resetCurrentPlan();
        }
    }
    
    // JUCE prepareToPlay integration
    void handlePrepareToPlay(double sampleRate, int samplesPerBlock)
    {
        EngineConfig newConfig = currentConfig_;
        newConfig.sampleRate = sampleRate;
        newConfig.maxBlockSize = samplesPerBlock;
        
        // Always reconfigure on prepareToPlay (safe allocation point)
        standbyPlan_.initialize(newConfig);
        std::swap(activePlan_, standbyPlan_);
        currentConfig_ = activePlan_.config;
    }
    
    // Get latency samples for current configuration
    int getCurrentLatencySamples() const noexcept
    {
        // Use single-source-of-truth constants to match prepareToPlay()
        // STFT overlap-add reconstruction delay
        return AtlasConfig::FFT_SIZE - AtlasConfig::HOP_SIZE; // e.g. 512-128 = 384
    }
    
private:
    // Double-buffered FFT plans
    FFTPlan activePlan_;                      // Currently used by audio thread
    FFTPlan standbyPlan_;                     // Being prepared for swap
    
    // Configuration state
    EngineConfig currentConfig_;              // Currently active configuration
    EngineConfig pendingConfig_;              // Requested configuration
    
    // Atomic flags for thread coordination
    std::atomic<bool> reconfigRequested_{false};  // UI requested a reconfiguration
    std::atomic<bool> reconfigInProgress_{false}; // Standby plan ready to swap
};

/**
 * Helper class for managing multiple cached FFT objects
 * Avoids costly FFT construction by pre-caching common sizes
 */
class FFTCache
{
public:
    static FFTCache& getInstance()
    {
        static FFTCache instance;
        return instance;
    }
    
    // Get cached FFT for specific size (may allocate on first request)
    std::shared_ptr<juce::dsp::FFT> getForwardFFT(ReconfigManager::FFTSize size)
    {
        const int index = getSizeIndex(size);
        if (index < 0) return nullptr;
        
        if (!forwardFFTs_[index])
        {
            const int fftOrder = static_cast<int>(std::log2(static_cast<int>(size)));
            forwardFFTs_[index] = std::make_shared<juce::dsp::FFT>(fftOrder);
        }
        
        return forwardFFTs_[index];
    }
    
    std::shared_ptr<juce::dsp::FFT> getInverseFFT(ReconfigManager::FFTSize size)
    {
        const int index = getSizeIndex(size);
        if (index < 0) return nullptr;
        
        if (!inverseFFTs_[index])
        {
            const int fftOrder = static_cast<int>(std::log2(static_cast<int>(size)));
            inverseFFTs_[index] = std::make_shared<juce::dsp::FFT>(fftOrder);
        }
        
        return inverseFFTs_[index];
    }
    
private:
    static constexpr int NUM_SIZES = 4; // 256, 512, 1024, 2048
    std::array<std::shared_ptr<juce::dsp::FFT>, NUM_SIZES> forwardFFTs_;
    std::array<std::shared_ptr<juce::dsp::FFT>, NUM_SIZES> inverseFFTs_;
    
    int getSizeIndex(ReconfigManager::FFTSize size) const
    {
        switch (size)
        {
            case ReconfigManager::FFTSize::Size256:  return 0;
            case ReconfigManager::FFTSize::Size512:  return 1;
            case ReconfigManager::FFTSize::Size1024: return 2;
            case ReconfigManager::FFTSize::Size2048: return 3;
            default: return -1;
        }
    }
};