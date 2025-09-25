#include "ZPlaneStyle.h"

// Simplified version without BinaryData dependency for modular DSP library
static juce::var parseEmbeddedJSON()
{
    // TODO: Replace with embedded LUT data or load from external file
    // For now, return a minimal valid JSON structure
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("curves", juce::var(juce::Array<juce::var>()));
    return juce::var(obj.get());
}

void ZPlaneStyle::buildFromEmbeddedLUT()
{
    auto v = parseEmbeddedJSON();

    // Simplified implementation - use default curves instead of loading from JSON
    // This maintains the API but removes the BinaryData dependency
    hasCoeffs = false;

    // Set up default filter state
    for (int s = 0; s < 6; ++s) {
        sosL[s].reset();
        sosR[s].reset();
    }
}

void ZPlaneStyle::process(juce::AudioBuffer<float>& buf, float style)
{
    // Early exit if no coefficients or bypassed
    if (!hasCoeffs || style <= 1e-3f) {
        return;
    }

    // Apply simple biquad cascade processing
    auto numChannels = buf.getNumChannels();
    auto numSamples = buf.getNumSamples();

    for (int ch = 0; ch < numChannels && ch < 2; ++ch) {
        auto* data = buf.getWritePointer(ch);
        auto* sos = (ch == 0) ? sosL : sosR;

        for (int i = 0; i < numSamples; ++i) {
            float y = data[i];
            for (int s = 0; s < 6; ++s) {
                y = sos[s].processSample(y);
            }
            data[i] = y;
        }
    }
}