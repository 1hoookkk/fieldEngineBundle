#pragma once
#include <JuceHeader.h>
#include "PaintQueue.h"
#include "HarmonicQuantizer.h"
#include "../dsp/SpscRing.h"
#include "../dsp/VoicePool.h"

class SpectralSynthEngine
{
public:
    static SpectralSynthEngine& instance() noexcept
    {
        static SpectralSynthEngine s;
        return s;
    }

    void prepare(double sampleRate, int maxBlockSize) noexcept;
    void pushGestureRT(const PaintEvent& g) noexcept;
    void processAudioBlock(juce::AudioBuffer<float>& buffer, double sampleRate) noexcept;
    void releaseResources() noexcept;
    
    bool isInitialized() const noexcept { return initialized_; }

    void setHarmonicDepth(float v) noexcept { harmonicDepth_.store(v); }
    void setMasterGain(float v) noexcept { masterGain_.store(v); }
    void setNumPartials(int n) noexcept { numPartials_.store( juce::jlimit(1, kMaxPartials, n)); }
    void setMaxVoices(int v) noexcept { maxVoices_.store( juce::jlimit(1, kMaxVoices, v)); }
    void setTopNBands(int n) noexcept { topNBands_.store( juce::jlimit(1, kMaxBands, n)); }
    
    // Status queries
    size_t getQueueSize() const noexcept;
    bool isMulticoreActive() const noexcept { return false; } // Placeholder
    uint32_t getSeqFallbackCount() const noexcept { return 0; } // Placeholder
    
    // Paint data structure for stroke processing
    struct PaintData {
        float timeNorm = 0.0f;
        float freqNorm = 0.0f;
        float pressure = 0.0f;
        float velocity = 0.0f;
        juce::Colour color = juce::Colours::white;
        uint32_t timestamp = 0;
        float frequencyHz = 440.0f;
        float amplitude = 0.0f;
        float panPosition = 0.0f;
        int synthMode = 0;
    };
    
    // Paint stroke processing
    void processPaintStroke(const PaintData& data) noexcept;
    
    // Mask snapshot interface (placeholder for now)
    class MaskSnapshot {
    public:
        void setMaskBlend(float v) noexcept { maskBlend_.store(v); }
        void setMaskStrength(float v) noexcept { maskStrength_.store(v); }
        void setFeatherTime(float v) noexcept { featherTime_.store(v); }
        void setFeatherFreq(float v) noexcept { featherFreq_.store(v); }
        void setThreshold(float v) noexcept { threshold_.store(v); }
        void setProtectHarmonics(bool v) noexcept { protectHarmonics_.store(v ? 1 : 0); }
    private:
        std::atomic<float> maskBlend_{0.5f};
        std::atomic<float> maskStrength_{0.5f};
        std::atomic<float> featherTime_{0.1f};
        std::atomic<float> featherFreq_{0.5f};
        std::atomic<float> threshold_{0.5f};
        std::atomic<int> protectHarmonics_{1};
    };
    
    MaskSnapshot& getMaskSnapshot() noexcept { return maskSnapshot_; }

private:
    SpectralSynthEngine() noexcept;
    ~SpectralSynthEngine() = default;

    SpectralSynthEngine(const SpectralSynthEngine&) = delete;
    SpectralSynthEngine& operator=(const SpectralSynthEngine&) = delete;

    // Modern JUCE DSP-based voice management
    std::unique_ptr<VoicePool> voicePool_;
    
    // RT-safe SPSC queue using modern implementation
    struct InternalPaintEvent {
        float baseHz;
        float amplitude;
        float pan;
        uint16_t partials;
    };
    SpscRing<InternalPaintEvent, 1024> eventQueue_;

    // Engine state
    double sampleRate_ = 44100.0;
    int blockSize_ = 128;
    bool initialized_ = false;

    // RT-safe atomic parameters
    std::atomic<float> harmonicDepth_{0.8f};
    std::atomic<float> masterGain_{0.7f};
    std::atomic<int> numPartials_{16};
    std::atomic<int> maxVoices_{32};
    std::atomic<int> topNBands_{8};
    
    static constexpr int kMaxPartials = 64;
    static constexpr int kMaxVoices = 64;
    static constexpr int kMaxBands = 32;
    
    // Mask snapshot instance
    MaskSnapshot maskSnapshot_;
    
    // Convert from old PaintEvent format to modern format
    void convertAndEnqueueGesture(const PaintEvent& g) noexcept;
};