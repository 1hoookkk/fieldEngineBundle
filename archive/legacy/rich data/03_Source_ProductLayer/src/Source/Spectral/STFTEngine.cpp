#include "STFTEngine.h"
#include <algorithm>
#include <cmath>

namespace SpectralCanvas {
namespace spectral {

void STFTEngine::prepare(double sampleRate, const STFTConfig& config) {
    config_ = config;
    sampleRate_ = sampleRate;
    
    // For now, keep it simple - just validate config
    if (config_.fftSize <= 0 || config_.hopSize <= 0 || config_.channels <= 0) {
        return; // Invalid config
    }
    
    initialized_ = true;
}

void STFTEngine::reset() {
    // Simple reset - clear any internal state
    initialized_ = false;
}

void STFTEngine::process(const juce::AudioBuffer<float>& input, 
                        juce::AudioBuffer<float>& output,
                        void* userData) {
    // Simple implementation: passthrough for now
    // This ensures the build succeeds and audio flows
    
    if (!initialized_) {
        output.makeCopyOf(input);
        return;
    }
    
    // Ensure output buffer matches input
    if (output.getNumSamples() != input.getNumSamples() || 
        output.getNumChannels() != input.getNumChannels()) {
        output.setSize(input.getNumChannels(), input.getNumSamples(), false, false, true);
    }
    
    // For initial implementation: clean passthrough
    output.makeCopyOf(input);
    
    // TODO: Real STFT processing would go here
    // For now, this ensures compilation and basic functionality
}

} // namespace spectral
} // namespace SpectralCanvas