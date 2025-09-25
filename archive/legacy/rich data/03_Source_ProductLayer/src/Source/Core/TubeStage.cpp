#include "TubeStage.h"
#include <cmath>

void TubeStage::prepare(double sampleRate, int maxBlockSize)
{
    this->sampleRate = sampleRate;
    
    // Pre-allocate oversampling buffers (max 4x)
    oversampledBuffer.setSize(2, maxBlockSize * 4);
    tempBuffer.setSize(2, maxBlockSize);
    
    // Calculate smoothing coefficient for ~5ms
    const float smoothingTime = 0.005f;
    smoothingCoeff = std::exp(-1.0f / (smoothingTime * sampleRate));
    
    // Build auto-gain compensation LUT
    for (int i = 0; i < 256; ++i)
    {
        float driveDb = (i / 255.0f) * 24.0f;
        float driveLinear = juce::Decibels::decibelsToGain(driveDb);
        // Empirical compensation curve
        autoGainLUT[i] = 1.0f / std::sqrt(1.0f + driveLinear * 0.5f);
    }
    
    calculateFilterCoefficients();
    reset();
}

void TubeStage::reset()
{
    oversampledBuffer.clear();
    tempBuffer.clear();
    
    for (auto& state : upsampleStates)
    {
        state.x1 = state.x2 = state.y1 = state.y2 = 0.0f;
    }
    
    for (auto& state : downsampleStates)
    {
        state.x1 = state.x2 = state.y1 = state.y2 = 0.0f;
    }
    
    for (auto& state : toneStates)
    {
        state.x1 = state.x2 = state.y1 = state.y2 = 0.0f;
    }
    
    currentDrive = targetDrive;
    currentBias = targetBias;
    currentTone = targetTone;
    currentOutput = targetOutput;
}

void TubeStage::process(juce::AudioBuffer<float>& buffer)
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    
    if (numChannels == 0 || numSamples == 0)
        return;
    
    // Update smoothed parameters
    updateSmoothing();
    
    if (oversampleFactor > 1)
    {
        // Upsample
        upsample(buffer, oversampledBuffer);
        
        // Process at higher sample rate
        processSaturation(oversampledBuffer);
        
        // Downsample back
        downsample(oversampledBuffer, buffer);
    }
    else
    {
        // Process at native sample rate
        processSaturation(buffer);
    }
    
    // Apply tone control (tilt EQ)
    if (std::abs(currentTone) > 0.01f)
    {
        const float toneFreq = 1000.0f;
        const float toneGain = currentTone * 6.0f; // ±6dB
        
        // Simple shelf filter
        for (int ch = 0; ch < numChannels && ch < 2; ++ch)
        {
            float* channelData = buffer.getWritePointer(ch);
            FilterState& state = toneStates[ch];
            
            for (int i = 0; i < numSamples; ++i)
            {
                float input = channelData[i];
                
                // Apply tone filter (first-order shelf)
                float output = toneCoeffs.b0 * input + toneCoeffs.b1 * state.x1 - toneCoeffs.a1 * state.y1;
                
                state.x1 = input;
                state.y1 = output;
                
                // Mix dry/wet based on tone amount
                channelData[i] = input + (output - input) * std::abs(currentTone);
            }
        }
    }
    
    // Apply output gain
    const float outputGain = juce::Decibels::decibelsToGain(currentOutput);
    buffer.applyGain(outputGain);
}

void TubeStage::processSaturation(juce::AudioBuffer<float>& buffer)
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    
    const float driveGain = juce::Decibels::decibelsToGain(currentDrive);
    
    // Get auto-gain compensation
    float compensation = 1.0f;
    if (autoGainEnabled)
    {
        const int driveIndex = juce::jlimit(0, 255, (int)(currentDrive * 10.625f));
        compensation = autoGainLUT[driveIndex];
    }
    
    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* channelData = buffer.getWritePointer(ch);
        
        for (int i = 0; i < numSamples; ++i)
        {
            float sample = channelData[i];
            
            // Apply drive
            sample *= driveGain;
            
            // Cubic saturation with bias
            sample = cubicClip(sample, currentBias);
            
            // Additional smoothing with tanh
            sample = fastTanh(sample * 0.9f) * 1.111f;
            
            // Apply compensation
            sample *= compensation;
            
            channelData[i] = sample;
        }
    }
}

void TubeStage::upsample(const juce::AudioBuffer<float>& input, juce::AudioBuffer<float>& output)
{
    const int numChannels = input.getNumChannels();
    const int numSamples = input.getNumSamples();
    const int outputSamples = numSamples * oversampleFactor;
    
    output.clear();
    
    const FilterCoeffs& coeffs = upsampleCoeffs[oversampleFactor - 1];
    
    for (int ch = 0; ch < numChannels && ch < 2; ++ch)
    {
        const float* inputData = input.getReadPointer(ch);
        float* outputData = output.getWritePointer(ch);
        FilterState& state = upsampleStates[ch];
        
        for (int i = 0; i < numSamples; ++i)
        {
            float sample = inputData[i];
            
            // Zero-stuff and filter
            for (int j = 0; j < oversampleFactor; ++j)
            {
                float input = (j == 0) ? sample * oversampleFactor : 0.0f;
                
                // Butterworth lowpass filter
                float output = coeffs.b0 * input + coeffs.b1 * state.x1 + coeffs.b2 * state.x2
                             - coeffs.a1 * state.y1 - coeffs.a2 * state.y2;
                
                state.x2 = state.x1;
                state.x1 = input;
                state.y2 = state.y1;
                state.y1 = output;
                
                *outputData++ = output;
            }
        }
    }
}

void TubeStage::downsample(const juce::AudioBuffer<float>& input, juce::AudioBuffer<float>& output)
{
    const int numChannels = input.getNumChannels();
    const int inputSamples = input.getNumSamples();
    const int outputSamples = inputSamples / oversampleFactor;
    
    const FilterCoeffs& coeffs = downsampleCoeffs[oversampleFactor - 1];
    
    for (int ch = 0; ch < numChannels && ch < 2; ++ch)
    {
        const float* inputData = input.getReadPointer(ch);
        float* outputData = output.getWritePointer(ch);
        FilterState& state = downsampleStates[ch];
        
        for (int i = 0; i < outputSamples; ++i)
        {
            // Apply anti-aliasing filter and decimate
            float sum = 0.0f;
            
            for (int j = 0; j < oversampleFactor; ++j)
            {
                float sample = *inputData++;
                
                // Butterworth lowpass filter
                float filtered = coeffs.b0 * sample + coeffs.b1 * state.x1 + coeffs.b2 * state.x2
                               - coeffs.a1 * state.y1 - coeffs.a2 * state.y2;
                
                state.x2 = state.x1;
                state.x1 = sample;
                state.y2 = state.y1;
                state.y1 = filtered;
                
                if (j == 0)
                    sum = filtered;
            }
            
            outputData[i] = sum;
        }
    }
}

void TubeStage::calculateFilterCoefficients()
{
    // Calculate Butterworth coefficients for each oversampling factor
    for (int factor = 1; factor <= 4; ++factor)
    {
        // Cutoff at Nyquist/2 of original rate
        const float cutoff = 0.5f / factor;
        const float wc = std::tan(juce::MathConstants<float>::pi * cutoff);
        const float wc2 = wc * wc;
        const float sqrt2 = std::sqrt(2.0f);
        
        // 2nd order Butterworth
        const float norm = 1.0f / (wc2 + sqrt2 * wc + 1.0f);
        
        FilterCoeffs& up = upsampleCoeffs[factor - 1];
        up.b0 = wc2 * norm;
        up.b1 = 2.0f * wc2 * norm;
        up.b2 = wc2 * norm;
        up.a1 = 2.0f * (wc2 - 1.0f) * norm;
        up.a2 = (wc2 - sqrt2 * wc + 1.0f) * norm;
        
        // Same coefficients for downsampling
        downsampleCoeffs[factor - 1] = up;
    }
    
    // Tone control coefficients (1st order shelf at 1kHz)
    const float toneFreq = 1000.0f / sampleRate;
    const float toneCutoff = std::tan(juce::MathConstants<float>::pi * toneFreq);
    const float toneNorm = 1.0f / (1.0f + toneCutoff);
    
    toneCoeffs.b0 = toneCutoff * toneNorm;
    toneCoeffs.b1 = toneCutoff * toneNorm;
    toneCoeffs.a1 = (toneCutoff - 1.0f) * toneNorm;
}

void TubeStage::updateSmoothing()
{
    currentDrive = smoothingCoeff * currentDrive + (1.0f - smoothingCoeff) * targetDrive;
    currentBias = smoothingCoeff * currentBias + (1.0f - smoothingCoeff) * targetBias;
    currentTone = smoothingCoeff * currentTone + (1.0f - smoothingCoeff) * targetTone;
    currentOutput = smoothingCoeff * currentOutput + (1.0f - smoothingCoeff) * targetOutput;
}