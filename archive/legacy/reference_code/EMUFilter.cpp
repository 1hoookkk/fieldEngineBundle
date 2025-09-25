/**
 * EMU Audity-Style Filter Implementation
 * Mathematical modeling of the legendary SSM2040-inspired multimode filter
 */

#include "EMUFilter.h"
#include <algorithm>
#include <cmath>
#include <complex>

//=============================================================================
// EMUFilterCore Implementation

EMUFilterCore::EMUFilterCore()
{
    reset();
}

void EMUFilterCore::prepareToPlay(double sampleRate_)
{
    sampleRate = sampleRate_;
    nyquistFreq = (float)(sampleRate * 0.5);
    
    // Reset filter state
    reset();
    
    // Update coefficients with current settings
    updateCoefficients();
}

float EMUFilterCore::processSample(float input)
{
    // Apply drive/saturation before filtering (EMU-style)
    float drivenInput = applySaturation(input * currentDrive);
    
    // SSM2040-inspired 4-pole filter structure
    // Using Chamberlin state-variable topology with modifications
    
    // Calculate feedback amount
    float fb = q + q / (1.0f - f);
    
    // Apply feedback
    float inputWithFeedback = drivenInput - delay4 * fb;
    
    // First stage (input mixing)
    float stage1 = f * inputWithFeedback + delay1;
    delay1 = f * inputWithFeedback - delay1 + stage1;
    
    // Second stage
    float stage2 = f * stage1 + delay2;  
    delay2 = f * stage1 - delay2 + stage2;
    
    // Third stage
    float stage3 = f * stage2 + delay3;
    delay3 = f * stage2 - delay3 + stage3;
    
    // Fourth stage
    float stage4 = f * stage3 + delay4;
    delay4 = f * stage3 - delay4 + stage4;
    
    // Select output based on filter type
    float output;
    switch (currentType)
    {
        case LowPass:
            output = stage4;  // 24dB/oct lowpass
            break;
            
        case HighPass:
            output = inputWithFeedback - stage1 * 4.0f + stage2 * 6.0f - stage3 * 4.0f + stage4;
            break;
            
        case BandPass:
            output = stage2 - stage4;  // 12dB/oct bandpass
            break;
            
        case Notch:
            output = inputWithFeedback - stage2 * 2.0f + stage4;
            break;
            
        case AllPass:
            output = inputWithFeedback - stage2 * 4.0f + stage4 * 2.0f;
            break;
            
        default:
            output = stage4;
            break;
    }
    
    // Apply vintage character if enabled
    if (vintageMode)
    {
        output = applyVintageCharacter(output);
    }
    
    return output;
}

void EMUFilterCore::processBlock(float* samples, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        samples[i] = processSample(samples[i]);
    }
}

void EMUFilterCore::reset()
{
    delay1 = delay2 = delay3 = delay4 = 0.0f;
    feedback = 0.0f;
}

void EMUFilterCore::setCutoffFrequency(float frequency)
{
    currentCutoff = juce::jlimit(20.0f, nyquistFreq * 0.9f, frequency);
    updateCoefficients();
}

void EMUFilterCore::setResonance(float resonance)
{
    currentResonance = juce::jlimit(0.0f, 0.99f, resonance);
    updateCoefficients();
}

void EMUFilterCore::setFilterType(FilterType type)
{
    currentType = type;
}

void EMUFilterCore::setDrive(float drive_)
{
    currentDrive = juce::jlimit(0.1f, 2.0f, drive_);
}

void EMUFilterCore::setKeyTracking(float amount)
{
    keyTrackAmount = juce::jlimit(0.0f, 1.0f, amount);
}

void EMUFilterCore::modulateCutoff(float modAmount)
{
    cutoffModulation = juce::jlimit(-2.0f, 2.0f, modAmount);
    
    // Apply modulation (±2 octaves)
    float modulatedCutoff = currentCutoff * std::pow(2.0f, cutoffModulation);
    f = frequencyToCoeff(modulatedCutoff);
}

void EMUFilterCore::modulateResonance(float modAmount)
{
    resonanceModulation = juce::jlimit(-0.5f, 0.5f, modAmount);
    
    // Apply resonance modulation
    float modulatedResonance = juce::jlimit(0.0f, 0.99f, currentResonance + resonanceModulation);
    q = modulatedResonance;
}

void EMUFilterCore::setVintageMode(bool enabled)
{
    vintageMode = enabled;
}

void EMUFilterCore::setFilterAge(float age)
{
    filterAge = juce::jlimit(0.0f, 1.0f, age);
}

void EMUFilterCore::setTemperatureDrift(float temp)
{
    temperatureDrift = juce::jlimit(-0.1f, 0.1f, temp);
}

EMUFilterCore::FrequencyResponse EMUFilterCore::calculateFrequencyResponse() const
{
    FrequencyResponse response;
    
    // Calculate frequency points (logarithmic spacing)
    for (int i = 0; i < 512; ++i)
    {
        float ratio = (float)i / 511.0f;
        response.frequencies[i] = 20.0f * std::pow(1000.0f, ratio); // 20Hz to 20kHz
    }
    
    // Simplified frequency response calculation without complex numbers
    for (int i = 0; i < 512; ++i)
    {
        float freq = response.frequencies[i];
        float normalizedFreq = freq / currentCutoff;
        
        // Simple filter response approximation
        float magnitude = 1.0f;
        
        switch (currentType)
        {
            case LowPass:
                if (normalizedFreq > 1.0f)
                {
                    // 24dB/oct rolloff
                    float rolloff = normalizedFreq;
                    magnitude = 1.0f / (rolloff * rolloff * rolloff * rolloff);
                }
                break;
                
            case HighPass:
                if (normalizedFreq < 1.0f)
                {
                    float rolloff = 1.0f / normalizedFreq;
                    magnitude = 1.0f / (rolloff * rolloff * rolloff * rolloff);
                }
                break;
                
            case BandPass:
                {
                    float distance = std::abs(std::log(normalizedFreq));
                    magnitude = 1.0f / (1.0f + distance * distance * 4.0f);
                }
                break;
                
            case Notch:
                {
                    float distance = std::abs(std::log(normalizedFreq));
                    if (distance < 0.1f)
                        magnitude = distance * 10.0f; // Notch
                    else
                        magnitude = 1.0f;
                }
                break;
                
            default:
                magnitude = 1.0f;
                break;
        }
        
        // Add resonance peaking near cutoff
        if (currentResonance > 0.1f && std::abs(std::log(normalizedFreq)) < 0.3f)
        {
            float peak = 1.0f + currentResonance * 3.0f;
            magnitude *= peak;
        }
        
        response.magnitude[i] = juce::jlimit(0.001f, 10.0f, magnitude);
        response.phase[i] = 0.0f; // Simplified - no phase calculation
    }
    
    return response;
}

void EMUFilterCore::updateCoefficients()
{
    // Convert cutoff frequency to coefficient
    f = frequencyToCoeff(currentCutoff);
    
    // Convert resonance to Q coefficient
    q = currentResonance;
    
    // Apply temperature drift (vintage character)
    if (vintageMode)
    {
        f *= (1.0f + temperatureDrift * 0.02f); // ±2% drift
        q *= (1.0f + temperatureDrift * 0.01f); // ±1% drift
    }
}

float EMUFilterCore::applySaturation(float input) const
{
    if (currentDrive <= 1.0f)
        return input;
        
    // Soft saturation (EMU-style)
    float driveAmount = currentDrive;
    float drivenSignal = input * driveAmount;
    
    // Asymmetric saturation for analog character
    if (drivenSignal > 0.0f)
    {
        return std::tanh(drivenSignal * 0.7f) / driveAmount;
    }
    else
    {
        return std::tanh(drivenSignal * 0.8f) / driveAmount; // Slightly different negative curve
    }
}

float EMUFilterCore::applyVintageCharacter(float input)
{
    // Add subtle random noise for vintage character
    float noise = vintageRandom.nextFloat() * 0.0001f * filterAge;
    
    // Add slight bit-crushing effect for digital vintage
    if (filterAge > 0.5f)
    {
        float crushed = std::floor(input * 4096.0f) / 4096.0f;
        input = input * (1.0f - filterAge * 0.1f) + crushed * (filterAge * 0.1f);
    }
    
    return input + noise;
}

float EMUFilterCore::frequencyToCoeff(float frequency) const
{
    // Convert frequency to filter coefficient
    // Clamped to prevent instability
    float normalizedFreq = frequency / (float)sampleRate;
    float coeff = 2.0f * std::sin(juce::MathConstants<float>::pi * normalizedFreq);
    return juce::jlimit(0.0001f, 0.99f, coeff);
}

//=============================================================================
// EMUDualFilter Implementation

EMUDualFilter::EMUDualFilter()
{
}

void EMUDualFilter::prepareToPlay(double sampleRate)
{
    filter1.prepareToPlay(sampleRate);
    filter2.prepareToPlay(sampleRate);
}

void EMUDualFilter::processBlock(juce::AudioSampleBuffer& buffer)
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    
    if (numChannels == 1 || routingMode != StereoSplit)
    {
        // Mono or non-stereo processing
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* channelData = buffer.getWritePointer(ch);
            
            switch (routingMode)
            {
                case Series:
                    // Filter1 → Filter2
                    filter1.processBlock(channelData, numSamples);
                    filter2.processBlock(channelData, numSamples);
                    break;
                    
                case Parallel:
                    // Filter1 + Filter2
                    {
                        // Create copy for second filter
                        std::vector<float> tempBuffer(channelData, channelData + numSamples);
                        
                        // Process both filters
                        filter1.processBlock(channelData, numSamples);
                        filter2.processBlock(tempBuffer.data(), numSamples);
                        
                        // Mix results
                        for (int i = 0; i < numSamples; ++i)
                        {
                            channelData[i] = channelData[i] * (1.0f - filterBalance) + 
                                           tempBuffer[i] * filterBalance;
                        }
                    }
                    break;
                    
                default:
                    filter1.processBlock(channelData, numSamples);
                    break;
            }
        }
    }
    else
    {
        // Stereo split mode
        if (numChannels >= 2)
        {
            filter1.processBlock(buffer.getWritePointer(0), numSamples);
            filter2.processBlock(buffer.getWritePointer(1), numSamples);
        }
    }
}

void EMUDualFilter::reset()
{
    filter1.reset();
    filter2.reset();
}

void EMUDualFilter::setRoutingMode(RoutingMode mode)
{
    routingMode = mode;
}

void EMUDualFilter::setFilterBalance(float balance)
{
    filterBalance = juce::jlimit(0.0f, 1.0f, balance);
}

void EMUDualFilter::linkFilters(bool linked)
{
    filtersLinked = linked;
    
    // If linking, copy Filter1 parameters to Filter2
    if (linked)
    {
        filter2.setCutoffFrequency(filter1.getCurrentCutoff());
        filter2.setResonance(filter1.getCurrentResonance());
        filter2.setFilterType(filter1.getCurrentType());
    }
}

//=============================================================================
// EMUFilter Implementation

EMUFilter::EMUFilter()
{
}

void EMUFilter::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    dualFilter.prepareToPlay(sampleRate);
}

void EMUFilter::processBlock(juce::AudioSampleBuffer& buffer)
{
    auto startTime = juce::Time::getHighResolutionTicks();
    
    // Update parameters from atomic values
    updateParameters();
    
    // Process through dual filter system
    dualFilter.processBlock(buffer);
    
    // Calculate CPU usage
    auto endTime = juce::Time::getHighResolutionTicks();
    auto elapsed = juce::Time::highResolutionTicksToSeconds(endTime - startTime);
    auto blockDuration = buffer.getNumSamples() / 44100.0; // Approximate
    cpuUsage.store((float)(elapsed / blockDuration));
}

void EMUFilter::releaseResources()
{
    dualFilter.reset();
}

void EMUFilter::setCutoff(float cutoff)
{
    atomicCutoff.store(juce::jlimit(0.0f, 1.0f, cutoff));
}

void EMUFilter::setResonance(float resonance)
{
    atomicResonance.store(juce::jlimit(0.0f, 1.0f, resonance));
}

void EMUFilter::setFilterType(int type)
{
    atomicFilterType.store(juce::jlimit(0, 4, type));
}

void EMUFilter::setDrive(float drive)
{
    atomicDrive.store(juce::jlimit(0.0f, 2.0f, drive));
}

void EMUFilter::setKeyTracking(float amount)
{
    keyTrackAmount = juce::jlimit(0.0f, 1.0f, amount);
}

void EMUFilter::setEnvelopeAmount(float amount)
{
    envelopeAmount = juce::jlimit(-2.0f, 2.0f, amount);
}

void EMUFilter::setLFOAmount(float amount)
{
    lfoAmount = juce::jlimit(-1.0f, 1.0f, amount);
}

void EMUFilter::setVelocityAmount(float amount)
{
    velocityAmount = juce::jlimit(0.0f, 1.0f, amount);
}

void EMUFilter::handlePaintStroke(float x, float y, float pressure, juce::Colour color)
{
    // Map paint coordinates to filter parameters based on mapping settings
    switch (xAxisMapping)
    {
        case 0: // Cutoff
            setCutoff(x);
            break;
        case 1: // Resonance
            setResonance(x);
            break;
        case 2: // Drive
            setDrive(x * 2.0f);
            break;
    }
    
    switch (yAxisMapping)
    {
        case 0: // Cutoff
            setCutoff(y);
            break;
        case 1: // Filter Type
            setFilterType((int)(y * 4.99f)); // Map to 0-4
            break;
        case 2: // Resonance
            setResonance(y);
            break;
    }
    
    switch (pressureMapping)
    {
        case 0: // Drive
            setDrive(pressure * 2.0f);
            break;
        case 1: // Resonance
            setResonance(pressure);
            break;
        case 2: // Cutoff modulation
            // Apply real-time cutoff modulation
            dualFilter.getFilter1().modulateCutoff((pressure - 0.5f) * 2.0f);
            break;
    }
}

void EMUFilter::setPaintMapping(int xMap, int yMap, int pressureMap)
{
    xAxisMapping = juce::jlimit(0, 2, xMap);
    yAxisMapping = juce::jlimit(0, 2, yMap);
    pressureMapping = juce::jlimit(0, 2, pressureMap);
}

void EMUFilter::enableDualFilter(bool enabled)
{
    dualFilterEnabled.store(enabled);
    
    if (enabled)
    {
        dualFilter.setRoutingMode(EMUDualFilter::Series);
    }
}

void EMUFilter::setFilterRouting(int routing)
{
    auto mode = static_cast<EMUDualFilter::RoutingMode>(juce::jlimit(0, 2, routing));
    dualFilter.setRoutingMode(mode);
}

void EMUFilter::setVintageMode(bool enabled)
{
    dualFilter.getFilter1().setVintageMode(enabled);
    dualFilter.getFilter2().setVintageMode(enabled);
}

void EMUFilter::setFilterAge(float age)
{
    dualFilter.getFilter1().setFilterAge(age);
    dualFilter.getFilter2().setFilterAge(age);
}

EMUFilter::FilterStatus EMUFilter::getFilterStatus() const
{
    FilterStatus status;
    status.currentCutoff = atomicCutoff.load();
    status.currentResonance = atomicResonance.load();
    status.currentType = atomicFilterType.load();
    status.cpuUsage = cpuUsage.load();
    status.isProcessing = true; // Could be more sophisticated
    
    return status;
}

void EMUFilter::getFrequencyResponse(float* magnitudes, float* frequencies, int numPoints) const
{
    // Simple frequency response calculation without copying filter
    for (int i = 0; i < numPoints; ++i)
    {
        float ratio = (float)i / (numPoints - 1);
        frequencies[i] = 20.0f * std::pow(1000.0f, ratio); // 20Hz to 20kHz
        
        // Simple lowpass response approximation
        float cutoff = mapPaintToCutoff(atomicCutoff.load());
        float normalizedFreq = frequencies[i] / cutoff;
        
        if (normalizedFreq > 1.0f)
        {
            magnitudes[i] = 1.0f / (normalizedFreq * normalizedFreq); // Simple rolloff
        }
        else
        {
            magnitudes[i] = 1.0f;
        }
    }
}

void EMUFilter::updateParameters()
{
    // Convert normalized parameters to actual values
    float cutoffHz = mapPaintToCutoff(atomicCutoff.load());
    float resonance = atomicResonance.load();
    int filterType = atomicFilterType.load();
    float drive = atomicDrive.load();
    
    // Update both filters
    dualFilter.getFilter1().setCutoffFrequency(cutoffHz);
    dualFilter.getFilter1().setResonance(resonance);
    dualFilter.getFilter1().setFilterType(static_cast<EMUFilterCore::FilterType>(filterType));
    dualFilter.getFilter1().setDrive(drive);
    
    if (dualFilterEnabled.load())
    {
        dualFilter.getFilter2().setCutoffFrequency(cutoffHz);
        dualFilter.getFilter2().setResonance(resonance);
        dualFilter.getFilter2().setFilterType(static_cast<EMUFilterCore::FilterType>(filterType));
        dualFilter.getFilter2().setDrive(drive);
    }
}

float EMUFilter::mapPaintToCutoff(float value) const
{
    // Logarithmic mapping: 20Hz to 20kHz
    return 20.0f * std::pow(1000.0f, value);
}

float EMUFilter::mapPaintToResonance(float value) const
{
    // Direct linear mapping with safety limit
    return juce::jlimit(0.0f, 0.95f, value);
}

float EMUFilter::mapPaintToDrive(float value) const
{
    // Linear mapping 0.1 to 2.0
    return 0.1f + value * 1.9f;
}

int EMUFilter::mapPaintToType(float value) const
{
    return juce::jlimit(0, 4, (int)(value * 4.99f));
}