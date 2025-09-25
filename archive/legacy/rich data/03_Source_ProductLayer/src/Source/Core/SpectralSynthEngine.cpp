#include "SpectralSynthEngine.h"
#include "../dsp/Voice.h"

using namespace juce;

SpectralSynthEngine::SpectralSynthEngine() noexcept
{
    // Modern JUCE-based initialization - no lookup tables needed
}

void SpectralSynthEngine::prepare(double sampleRate, int maxBlockSize) noexcept
{
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;
    blockSize_ = juce::jmax(16, maxBlockSize);
    
    // Initialize modern VoicePool with JUCE DSP components
    const int maxVoices = maxVoices_.load();
    const int maxPartials = numPartials_.load();
    
    voicePool_ = std::make_unique<VoicePool>(maxVoices);
    voicePool_->prepare(sampleRate_, blockSize_, maxPartials);
    
    initialized_ = true;
}

void SpectralSynthEngine::pushGestureRT(const PaintEvent& g) noexcept
{
    convertAndEnqueueGesture(g);
}

void SpectralSynthEngine::convertAndEnqueueGesture(const PaintEvent& g) noexcept
{
    // Convert old PaintEvent format to modern format with harmonic quantization
    const float minFreq = 80.0f;
    const float maxFreq = 2000.0f;
    const float baseHz = minFreq + g.ny * (maxFreq - minFreq);
    
    // Apply harmonic quantization based on pressure
    const double sigmaCents = scp::pressureToSigmaCents(g.pressure, 200.0, 8.0);
    double weightCents;
    const float quantizedHz = static_cast<float>(
        scp::computeSnappedFrequencyCmaj(baseHz, sigmaCents, weightCents)
    );
    
    InternalPaintEvent event {
        quantizedHz,                                    // baseHz - quantized to C major
        juce::jlimit(0.1f, 1.0f, g.pressure),         // amplitude based on pressure
        (g.nx - 0.5f) * 2.0f,                          // pan from X position (-1 to +1)
        static_cast<uint16_t>(8 + g.pressure * 8)      // partials based on pressure (8-16)
    };
    
    eventQueue_.push(event);
}

void SpectralSynthEngine::processAudioBlock(juce::AudioBuffer<float>& buffer, double /*sampleRate*/) noexcept
{
    if (!initialized_ || !voicePool_)
        return;
        
    // Clear buffer for synthesis (we're generating, not processing input)
    buffer.clear();
    
    // Consume queued paint events and spawn voices
    InternalPaintEvent event;
    while (eventQueue_.pop(event))
    {
        if (auto* voice = voicePool_->allocate())
        {
            voice->noteOn(event.baseHz, event.amplitude, event.partials, event.pan);
        }
    }
    
    // Render all active voices into buffer
    voicePool_->render(buffer);
    
    // Apply master gain
    const float masterGain = masterGain_.load();
    buffer.applyGain(masterGain);
}

void SpectralSynthEngine::releaseResources() noexcept
{
    voicePool_.reset();
    initialized_ = false;
}

size_t SpectralSynthEngine::getQueueSize() const noexcept
{
    return eventQueue_.size();
}

void SpectralSynthEngine::processPaintStroke(const PaintData& data) noexcept
{
    // Convert PaintData to InternalPaintEvent and enqueue
    InternalPaintEvent event {
        data.frequencyHz,                               // baseHz
        juce::jlimit(0.1f, 1.0f, data.amplitude),     // amplitude
        data.panPosition,                               // pan
        static_cast<uint16_t>(8 + data.pressure * 8)  // partials based on pressure (8-16)
    };
    
    eventQueue_.push(event);
}

// Legacy code removed - now using modern JUCE DSP VoicePool