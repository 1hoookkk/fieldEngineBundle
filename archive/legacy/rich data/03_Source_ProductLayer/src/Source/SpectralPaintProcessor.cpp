#include "SpectralPaintProcessor.h"
#include <algorithm>

SpectralPaintProcessor::SpectralPaintProcessor()
    : spectralNode_(std::make_unique<nodes::SpectralPaintNode>())
{
}

void SpectralPaintProcessor::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate_ = spec.sampleRate;
    maxBlockSize_ = static_cast<int>(spec.maximumBlockSize);
    
    // Prepare the spectral node directly
    spectralNode_->prepare(spec);
    
    // Mark as initialized
    initialized_.store(true, std::memory_order_release);
}

void SpectralPaintProcessor::reset() noexcept
{
    if (!initialized_.load(std::memory_order_acquire))
        return;
        
    spectralNode_->reset();
}

void SpectralPaintProcessor::processBlock(const juce::dsp::ProcessContextReplacing<float>& context) noexcept
{
    // Quick bypass check
    if (bypassed_.load(std::memory_order_relaxed))
    {
        // Pass input directly to output
        auto& inputBlock = context.getInputBlock();
        auto& outputBlock = context.getOutputBlock();
        
        const auto numSamples = inputBlock.getNumSamples();
        const auto numChannels = std::min(inputBlock.getNumChannels(), outputBlock.getNumChannels());
        
        for (size_t ch = 0; ch < numChannels; ++ch)
        {
            for (size_t sample = 0; sample < numSamples; ++sample)
            {
                outputBlock.setSample(static_cast<int>(ch), static_cast<int>(sample), 
                                    inputBlock.getSample(static_cast<int>(ch), static_cast<int>(sample)));
            }
        }
        return;
    }
    
    // Check initialization
    if (!initialized_.load(std::memory_order_acquire))
    {
        // Zero output if not initialized
        context.getOutputBlock().clear();
        return;
    }
    
    // Process through spectral paint node directly
    spectralNode_->process(context);
}

bool SpectralPaintProcessor::pushPaintEvent(float y, float intensity, uint32_t timestampMs) noexcept
{
    if (!validateParameters(y, intensity))
        return false;
        
    if (!initialized_.load(std::memory_order_acquire))
        return false;
    
    // Create lightweight mask event
    rt::MaskColumnLite mask(y, intensity, timestampMs);
    
    // Push to spectral node's queue
    return spectralNode_->pushMask(mask);
}

void SpectralPaintProcessor::setPinkBedEnabled(bool enabled) noexcept
{
    if (spectralNode_)
    {
        spectralNode_->setPinkBed(enabled);
    }
}

void SpectralPaintProcessor::setBrushRadius(int radius) noexcept
{
    // Clamp to reasonable range
    const int clampedRadius = std::max(1, std::min(32, radius));
    
    if (spectralNode_)
    {
        spectralNode_->setBrushRadius(clampedRadius);
    }
}

int SpectralPaintProcessor::getFFTOrder() const noexcept
{
    return spectralNode_ ? spectralNode_->getFFTOrder() : 0;
}

size_t SpectralPaintProcessor::getFFTSize() const noexcept
{
    return spectralNode_ ? spectralNode_->getFFTSize() : 0;
}

int SpectralPaintProcessor::getHopSize() const noexcept
{
    return spectralNode_ ? spectralNode_->getHopSize() : 0;
}

int SpectralPaintProcessor::getNumBins() const noexcept
{
    return spectralNode_ ? spectralNode_->getNumBins() : 0;
}

size_t SpectralPaintProcessor::getMaskQueueDepth() const noexcept
{
    return spectralNode_ ? spectralNode_->getMaskQueueDepth() : 0;
}

size_t SpectralPaintProcessor::getMaskDropCount() const noexcept
{
    return spectralNode_ ? spectralNode_->getMaskDropCount() : 0;
}

bool SpectralPaintProcessor::validateParameters(float y, float intensity) const noexcept
{
    // Check Y coordinate is in valid range [0,1]
    if (y < 0.0f || y > 1.0f)
        return false;
        
    // Check intensity is in valid range [0,1]
    if (intensity < 0.0f || intensity > 1.0f)
        return false;
        
    // Check for NaN/inf
    if (!std::isfinite(y) || !std::isfinite(intensity))
        return false;
        
    return true;
}