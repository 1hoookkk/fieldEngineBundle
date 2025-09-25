#include "SpectralSynthEngineStub.h"

using namespace juce;

SpectralSynthEngineStub::SpectralSynthEngineStub()
{
    // Initialize all oscillators to inactive state
    for (auto& osc : oscillators_)
    {
        osc.reset();
    }
    
    lastProcessTime_ = Time::getCurrentTime();
}

SpectralSynthEngineStub::~SpectralSynthEngineStub()
{
    releaseResources();
}

void SpectralSynthEngineStub::prepareToPlay(double sampleRate, int samplesPerBlock, int numChannels)
{
    sampleRate_.store(sampleRate);
    
    // Reset all oscillators
    for (auto& osc : oscillators_)
    {
        osc.reset();
    }
    
    activeOscillatorCount_.store(0);
    nextOscillatorIndex_.store(0);
    currentCpuLoad_.store(0.0f);
    testTonePhase_ = 0.0f;
    
    lastProcessTime_ = Time::getCurrentTime();
}

void SpectralSynthEngineStub::processBlock(AudioBuffer<float>& buffer, SpectralPaintQueue* paintQueue)
{
    auto startTime = Time::getCurrentTime();
    
    // Clear the output buffer
    buffer.clear();
    
    // Safety check: prevent processing if sample rate is invalid
    double sampleRateValue = sampleRate_.load();
    if (sampleRateValue < 8000.0 || sampleRateValue > 192000.0)
    {
        return; // Skip processing with invalid sample rate
    }
    
    // Process incoming paint gestures
    if (paintQueue)
    {
        processPaintGestures(paintQueue);
    }
    
    // Render test tone if enabled (for debugging audio pipeline)
    if (testToneEnabled_.load())
    {
        float testFreq = testToneFrequency_.load();
        double sampleRateValue = sampleRate_.load();
        float phaseIncrement = static_cast<float>(testFreq * 2.0 * MathConstants<double>::pi / sampleRateValue);
        float testGain = 0.1f; // Quiet test tone
        
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            float testSample = std::sin(testTonePhase_) * testGain;
            testTonePhase_ += phaseIncrement;
            
            if (testTonePhase_ > 2.0f * MathConstants<float>::pi)
                testTonePhase_ -= 2.0f * MathConstants<float>::pi;
            
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                buffer.addSample(channel, sample, testSample);
            }
        }
    }
    
    // Render spectral oscillators from paint gestures
    renderOscillators(buffer);
    
    // Apply master gain with safety limiting
    float gain = masterGain_.load();
    gain = jlimit(0.0f, 0.8f, gain); // Hard limit to prevent clipping
    buffer.applyGain(gain);
    
    // Update performance metrics
    updatePerformanceMetrics();
}

void SpectralSynthEngineStub::releaseResources()
{
    // Reset all oscillators
    for (auto& osc : oscillators_)
    {
        osc.reset();
    }
    
    activeOscillatorCount_.store(0);
    currentCpuLoad_.store(0.0f);
}

void SpectralSynthEngineStub::setFrequencyRange(float minHz, float maxHz) noexcept
{
    minFrequencyHz_.store(jmax(20.0f, minHz));
    maxFrequencyHz_.store(jmin(22000.0f, jmax(minHz + 10.0f, maxHz)));
}

void SpectralSynthEngineStub::setGain(float gain) noexcept
{
    masterGain_.store(jlimit(0.0f, 1.0f, gain));
}

void SpectralSynthEngineStub::setDecayRate(float decayRate) noexcept
{
    defaultDecayRate_.store(jlimit(0.9f, 0.999f, decayRate));
}

void SpectralSynthEngineStub::processPaintGestures(SpectralPaintQueue* paintQueue) noexcept
{
    PaintEvent event;
    
    // Process up to 32 paint events per block to avoid overwhelming the audio thread
    int processedEvents = 0;
    constexpr int maxEventsPerBlock = 32;
    
    while (processedEvents < maxEventsPerBlock && paintQueue->pop(event))
    {
        ++processedEvents;
        
        // Convert normalized coordinates to synthesis parameters
        float frequency = normalizedYToFrequency(event.ny);
        float amplitude = pressureToAmplitude(event.pressure);
        float decay = defaultDecayRate_.load();
        
        // Find a free oscillator and trigger it
        if (auto* osc = findFreeOscillator())
        {
            osc->trigger(frequency, amplitude, decay);
        }
    }
}

void SpectralSynthEngineStub::renderOscillators(AudioBuffer<float>& buffer) noexcept
{
    double sampleRateValue = sampleRate_.load();
    int activeCount = 0;
    
    // Render each active oscillator
    for (auto& osc : oscillators_)
    {
        if (!osc.active.load()) continue;
        
        ++activeCount;
        
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            float oscillatorSample = osc.renderSample(sampleRateValue);
            
            // Add to all channels (simple mono-to-stereo)
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                buffer.addSample(channel, sample, oscillatorSample);
            }
        }
    }
    
    activeOscillatorCount_.store(activeCount);
}

SpectralSynthEngineStub::SimpleOscillator* SpectralSynthEngineStub::findFreeOscillator() noexcept
{
    // Simple round-robin allocation
    for (int attempts = 0; attempts < kMaxOscillators; ++attempts)
    {
        int index = nextOscillatorIndex_.load();
        nextOscillatorIndex_.store((index + 1) % kMaxOscillators);
        
        auto& osc = oscillators_[index];
        if (!osc.active.load())
        {
            return &osc;
        }
    }
    
    // If no free oscillator, steal the oldest one (round-robin)
    int index = nextOscillatorIndex_.load();
    nextOscillatorIndex_.store((index + 1) % kMaxOscillators);
    return &oscillators_[index];
}

void SpectralSynthEngineStub::updatePerformanceMetrics() noexcept
{
    auto currentTime = Time::getCurrentTime();
    auto timeDiff = currentTime - lastProcessTime_;
    
    // Simple CPU load estimation (this is very approximate)
    double processingTimeMs = timeDiff.inMilliseconds();
    double expectedTimeMs = 1000.0 / 30.0; // Assuming ~30 calls per second
    
    float cpuLoad = static_cast<float>(processingTimeMs / expectedTimeMs);
    currentCpuLoad_.store(jlimit(0.0f, 1.0f, cpuLoad));
    
    lastProcessTime_ = currentTime;
}

float SpectralSynthEngineStub::normalizedYToFrequency(float normalizedY) const noexcept
{
    // Logarithmic frequency mapping (more musical)
    float minFreq = minFrequencyHz_.load();
    float maxFreq = maxFrequencyHz_.load();
    
    // Clamp input to valid range
    float clampedY = jlimit(0.0f, 1.0f, normalizedY);
    
    // Logarithmic interpolation for more musical frequency distribution
    float logMin = std::log(minFreq);
    float logMax = std::log(maxFreq);
    float logFreq = logMin + clampedY * (logMax - logMin);
    
    return std::exp(logFreq);
}

float SpectralSynthEngineStub::pressureToAmplitude(float pressure) const noexcept
{
    // Smooth curve for pressure-to-amplitude mapping
    float clampedPressure = jlimit(0.0f, 1.0f, pressure);
    
    // Power curve for more natural feel (pressure^0.7)
    return std::pow(clampedPressure, 0.7f) * 0.4f; // Max 0.4 amplitude to prevent clipping
}

// SimpleOscillator implementation
void SpectralSynthEngineStub::SimpleOscillator::reset() noexcept
{
    active.store(false);
    frequency.store(440.0f);
    amplitude.store(0.0f);
    decayRate.store(0.95f);
    phase = 0.0f;
    currentAmplitude = 0.0f;
    decayCounter = 0;
}

float SpectralSynthEngineStub::SimpleOscillator::renderSample(double sampleRate) noexcept
{
    if (!active.load()) return 0.0f;
    
    // Generate sine wave sample
    float freq = frequency.load();
    float phaseIncrement = static_cast<float>(freq * 2.0 * MathConstants<double>::pi / sampleRate);
    
    float sample = std::sin(phase) * currentAmplitude;
    phase += phaseIncrement;
    
    // Keep phase in range
    if (phase > 2.0f * MathConstants<float>::pi)
        phase -= 2.0f * MathConstants<float>::pi;
    
    // Apply decay
    float decay = decayRate.load();
    currentAmplitude *= decay;
    ++decayCounter;
    
    // Deactivate when amplitude is very low or time limit reached
    if (currentAmplitude < 0.001f || decayCounter > kDecayTime)
    {
        active.store(false);
        return 0.0f;
    }
    
    return sample;
}

void SpectralSynthEngineStub::SimpleOscillator::trigger(float freq, float amp, float decay) noexcept
{
    frequency.store(freq);
    amplitude.store(amp);
    decayRate.store(decay);
    
    // Audio thread only - no atomics needed for these
    currentAmplitude = amp;
    decayCounter = 0;
    phase = 0.0f;
    
    // Activate last to ensure all parameters are set
    active.store(true);
}