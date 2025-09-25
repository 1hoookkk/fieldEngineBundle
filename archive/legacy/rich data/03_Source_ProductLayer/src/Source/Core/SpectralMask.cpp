#include "SpectralMask.h"
#include <cmath>
#include <algorithm>

//==============================================================================
SpectralMask::SpectralMask()
{
    // Initialize FFT processing
    fft = std::make_unique<juce::dsp::FFT>(FFT_ORDER);
    window = std::make_unique<juce::dsp::WindowingFunction<float>>(FFT_SIZE, juce::dsp::WindowingFunction<float>::hann);
    
    // Allocate buffers
    fftData.resize(FFT_SIZE * 2); // Complex data requires 2x size
    tempAudioBuffer.resize(FFT_SIZE);
    displayMagnitudes.resize(SPECTRUM_BINS);
}

SpectralMask::~SpectralMask() = default;

//==============================================================================
void SpectralMask::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    this->sampleRate = sampleRate;
    this->samplesPerBlock = samplesPerBlock;
    
    // Adjust frame size based on sample rate for good temporal resolution
    frameSize = static_cast<int>(sampleRate * 0.01); // 10ms frames
    frameSize = juce::nextPowerOfTwo(frameSize);
    frameSize = std::min(frameSize, FFT_SIZE);
    
    // Clear any existing analysis
    clearAnalysis();
}

void SpectralMask::releaseResources()
{
    clearAnalysis();
}

//==============================================================================
void SpectralMask::analyzeSample(const juce::AudioBuffer<float>& sampleBuffer, int channel)
{
    if (sampleBuffer.getNumSamples() == 0 || channel >= sampleBuffer.getNumChannels())
        return;
    
    clearAnalysis();
    
    const float* sampleData = sampleBuffer.getReadPointer(channel);
    const int numSamples = sampleBuffer.getNumSamples();
    const int maxFrames = std::min(numSamples / frameSize, MAX_ANALYSIS_LENGTH / frameSize);
    
    spectralFrames.reserve(maxFrames);
    
    // Analyze sample in overlapping frames
    for (int frameStart = 0; frameStart < numSamples - FFT_SIZE; frameStart += frameSize)
    {
        if (spectralFrames.size() >= maxFrames)
            break;
            
        SpectralFrame frame;
        performFFT(sampleData + frameStart, frame);
        calculateSpectralFeatures(frame);
        
        // Smooth with previous frame
        if (!spectralFrames.empty())
            smoothSpectralFrame(frame, spectralFrames.back());
        
        spectralFrames.push_back(frame);
    }
    
    DBG("SpectralMask: Analyzed " << spectralFrames.size() << " frames from sample");
}

void SpectralMask::clearAnalysis()
{
    spectralFrames.clear();
    maskPosition = 0.0f;
    currentEnergy = 0.0f;
    frameCounter = 0;
}

//==============================================================================
void SpectralMask::processBlock(juce::AudioBuffer<float>& buffer, const juce::AudioBuffer<float>& maskSource)
{
    if (maskType == MaskType::Off || !hasAnalyzedSample())
        return;
    
    updateMaskPosition();
    
    const SpectralFrame* currentFrame = getCurrentFrame();
    if (currentFrame == nullptr)
        return;
    
    // Apply the appropriate masking technique
    switch (maskType)
    {
        case MaskType::SpectralGate:
            applySpectralGate(buffer, *currentFrame);
            break;
            
        case MaskType::SpectralFilter:
            applySpectralFilter(buffer, *currentFrame);
            break;
            
        case MaskType::RhythmicGate:
            applyRhythmicGate(buffer, *currentFrame);
            break;
            
        case MaskType::SpectralMorph:
            // More complex morphing - could be implemented later
            applySpectralFilter(buffer, *currentFrame);
            break;
            
        default:
            break;
    }
}

//==============================================================================
void SpectralMask::performFFT(const float* timeData, SpectralFrame& frame)
{
    // Copy time-domain data to FFT buffer
    std::copy(timeData, timeData + FFT_SIZE, fftData.begin());
    
    // Apply windowing
    window->multiplyWithWindowingTable(fftData.data(), FFT_SIZE);
    
    // Zero out imaginary part
    for (int i = 0; i < FFT_SIZE; ++i)
        fftData[FFT_SIZE + i] = 0.0f;
    
    // Perform FFT
    fft->performFrequencyOnlyForwardTransform(fftData.data());
    
    // SIMD-OPTIMIZED: Extract magnitudes and phases
    // Use vectorized operations for magnitude calculation where possible
    if (SPECTRUM_BINS >= 4) // Ensure we have enough data for vectorization
    {
        // Process in chunks of 4 for SIMD optimization
        const int vectorizedBins = SPECTRUM_BINS & ~3; // Round down to multiple of 4
        
        // Create temporary aligned buffers for SIMD operations
        alignas(16) float realParts[SPECTRUM_BINS];
        alignas(16) float imagParts[SPECTRUM_BINS];
        alignas(16) float realSquared[SPECTRUM_BINS];
        alignas(16) float imagSquared[SPECTRUM_BINS];
        
        // Deinterleave complex data into separate real/imaginary arrays
        for (int bin = 0; bin < SPECTRUM_BINS; ++bin)
        {
            realParts[bin] = fftData[bin * 2];
            imagParts[bin] = fftData[bin * 2 + 1];
        }
        
        // SIMD: Compute real² and imag² using vectorized operations
        juce::FloatVectorOperations::multiply(realSquared, realParts, realParts, vectorizedBins);
        juce::FloatVectorOperations::multiply(imagSquared, imagParts, imagParts, vectorizedBins);
        
        // SIMD: Add real² + imag²
        juce::FloatVectorOperations::add(realSquared, imagSquared, vectorizedBins);
        
        // Extract magnitudes - sqrt is not vectorized in JUCE, but we can optimize the loop
        for (int bin = 0; bin < vectorizedBins; ++bin)
        {
            frame.magnitudes[bin] = std::sqrt(realSquared[bin]);
            frame.phases[bin] = std::atan2(imagParts[bin], realParts[bin]);
        }
        
        // Handle remaining bins (if SPECTRUM_BINS is not divisible by 4)
        for (int bin = vectorizedBins; bin < SPECTRUM_BINS; ++bin)
        {
            float real = realParts[bin];
            float imag = imagParts[bin];
            frame.magnitudes[bin] = std::sqrt(real * real + imag * imag);
            frame.phases[bin] = std::atan2(imag, real);
        }
    }
    else
    {
        // Fallback for small spectrum sizes
        for (int bin = 0; bin < SPECTRUM_BINS; ++bin)
        {
            float real = fftData[bin * 2];
            float imag = fftData[bin * 2 + 1];
            
            frame.magnitudes[bin] = std::sqrt(real * real + imag * imag);
            frame.phases[bin] = std::atan2(imag, real);
        }
    }
}

void SpectralMask::applySpectralGate(juce::AudioBuffer<float>& buffer, const SpectralFrame& maskFrame)
{
    // Simple time-domain approximation of spectral gating
    // In a full implementation, this would use overlap-add STFT processing
    
    const float gateThreshold = sensitivity * 0.1f;
    const float overallGain = maskFrame.overallEnergy > gateThreshold ? maskStrength : 0.0f;
    
    // Apply gain based on spectral energy
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);
        
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            channelData[sample] *= overallGain;
        }
    }
    
    currentEnergy = maskFrame.overallEnergy;
}

void SpectralMask::applySpectralFilter(juce::AudioBuffer<float>& buffer, const SpectralFrame& maskFrame)
{
    // Frequency-dependent filtering based on spectral content
    // This is a simplified implementation - full spectral filtering would use STFT
    
    const float brightness = maskFrame.brightness;
    const float centroid = maskFrame.centroid;
    
    // Create simple high-pass/low-pass effect based on spectral centroid
    const float normalizedCentroid = centroid / (sampleRate * 0.5f); // Normalize to 0-1
    const float filterGain = 0.3f + 0.7f * normalizedCentroid * maskStrength;
    
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);
        
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            channelData[sample] *= filterGain;
        }
    }
    
    currentEnergy = maskFrame.overallEnergy;
}

void SpectralMask::applyRhythmicGate(juce::AudioBuffer<float>& buffer, const SpectralFrame& maskFrame)
{
    // Use overall energy envelope for rhythmic gating (great for hi-hats!)
    const float energyThreshold = sensitivity * 0.05f;
    const float gateAmount = maskFrame.overallEnergy > energyThreshold ? 1.0f : 0.0f;
    const float finalGain = (1.0f - maskStrength) + maskStrength * gateAmount;
    
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);
        
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            channelData[sample] *= finalGain;
        }
    }
    
    currentEnergy = maskFrame.overallEnergy;
}

//==============================================================================
void SpectralMask::updateMaskPosition()
{
    if (!hasAnalyzedSample())
        return;
    
    const float frameRate = sampleRate / frameSize;
    const float positionIncrement = (1.0f / frameRate) / timeStretch;
    
    maskPosition += positionIncrement / spectralFrames.size();
    
    // Loop the position
    if (maskPosition >= 1.0f)
        maskPosition -= 1.0f;
}

const SpectralMask::SpectralFrame* SpectralMask::getCurrentFrame() const
{
    if (!hasAnalyzedSample())
        return nullptr;
    
    const int frameIndex = static_cast<int>(maskPosition * spectralFrames.size());
    const int clampedIndex = juce::jlimit(0, static_cast<int>(spectralFrames.size()) - 1, frameIndex);
    
    return &spectralFrames[clampedIndex];
}

//==============================================================================
void SpectralMask::setFrequencyRange(float minHz, float maxHz)
{
    minFrequency = juce::jmax(20.0f, minHz);
    maxFrequency = juce::jmin(static_cast<float>(sampleRate * 0.5), maxHz);
}

float SpectralMask::binToFrequency(int bin) const
{
    return (bin * sampleRate) / (2.0f * SPECTRUM_BINS);
}

int SpectralMask::frequencyToBin(float frequency) const
{
    return static_cast<int>((frequency * 2.0f * SPECTRUM_BINS) / sampleRate);
}

//==============================================================================
void SpectralMask::smoothSpectralFrame(SpectralFrame& frame, const SpectralFrame& previousFrame)
{
    // SIMD-OPTIMIZED: Vectorized linear interpolation for magnitude smoothing
    // frame = (1 - smoothing) * frame + smoothing * previousFrame
    
    const float oneMinusSmoothing = 1.0f - smoothing;
    
    // Create temporary aligned buffers for SIMD operations
    alignas(16) float tempFrame[SPECTRUM_BINS];
    alignas(16) float tempPrevious[SPECTRUM_BINS];
    
    // Copy data to aligned buffers (necessary for some SIMD operations)
    std::copy(frame.magnitudes.begin(), frame.magnitudes.end(), tempFrame);
    std::copy(previousFrame.magnitudes.begin(), previousFrame.magnitudes.end(), tempPrevious);
    
    // SIMD: Scale current frame by (1 - smoothing)
    juce::FloatVectorOperations::multiply(tempFrame, oneMinusSmoothing, SPECTRUM_BINS);
    
    // SIMD: Scale previous frame by smoothing and add to current
    juce::FloatVectorOperations::addWithMultiply(tempFrame, tempPrevious, smoothing, SPECTRUM_BINS);
    
    // Copy result back to frame
    std::copy(tempFrame, tempFrame + SPECTRUM_BINS, frame.magnitudes.begin());
    
    // Smooth spectral features (scalar operations - not worth vectorizing for 3 values)
    frame.overallEnergy = smoothing * previousFrame.overallEnergy + 
                         oneMinusSmoothing * frame.overallEnergy;
    frame.centroid = smoothing * previousFrame.centroid + 
                    oneMinusSmoothing * frame.centroid;
    frame.brightness = smoothing * previousFrame.brightness + 
                      oneMinusSmoothing * frame.brightness;
}

void SpectralMask::calculateSpectralFeatures(SpectralFrame& frame)
{
    // Calculate overall energy
    frame.overallEnergy = 0.0f;
    for (int bin = 0; bin < SPECTRUM_BINS; ++bin)
    {
        frame.overallEnergy += frame.magnitudes[bin] * frame.magnitudes[bin];
    }
    frame.overallEnergy = std::sqrt(frame.overallEnergy / SPECTRUM_BINS);
    
    // Calculate spectral centroid (brightness measure)
    float weightedSum = 0.0f;
    float magnitudeSum = 0.0f;
    
    for (int bin = 1; bin < SPECTRUM_BINS; ++bin)
    {
        float frequency = binToFrequency(bin);
        float magnitude = frame.magnitudes[bin];
        
        weightedSum += frequency * magnitude;
        magnitudeSum += magnitude;
    }
    
    frame.centroid = magnitudeSum > 0.0f ? weightedSum / magnitudeSum : 0.0f;
    
    // Calculate brightness (high-frequency content)
    const int brightnessBin = frequencyToBin(3000.0f); // Above 3kHz
    float highFreqEnergy = 0.0f;
    
    for (int bin = brightnessBin; bin < SPECTRUM_BINS; ++bin)
    {
        highFreqEnergy += frame.magnitudes[bin] * frame.magnitudes[bin];
    }
    
    frame.brightness = frame.overallEnergy > 0.0f ? 
                      std::sqrt(highFreqEnergy) / frame.overallEnergy : 0.0f;
}

//==============================================================================
void SpectralMask::getSpectralDisplay(std::vector<float>& magnitudes, int displayBins) const
{
    magnitudes.resize(displayBins);
    std::fill(magnitudes.begin(), magnitudes.end(), 0.0f);
    
    const SpectralFrame* currentFrame = getCurrentFrame();
    if (currentFrame == nullptr)
        return;
    
    // Downsample spectrum for display
    const float binStep = static_cast<float>(SPECTRUM_BINS) / displayBins;
    
    for (int i = 0; i < displayBins; ++i)
    {
        const int sourceBin = static_cast<int>(i * binStep);
        if (sourceBin < SPECTRUM_BINS)
        {
            magnitudes[i] = currentFrame->magnitudes[sourceBin];
        }
    }
}