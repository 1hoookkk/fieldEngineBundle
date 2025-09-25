/******************************************************************************
 * File: CDPSpectralEngine.cpp
 * Description: Implementation of CDP-style spectral processing engine
 * 
 * Revolutionary real-time spectral processing system inspired by the legendary
 * Composer's Desktop Project, bringing decades of experimental music software
 * innovation into modern real-time paint-controlled workflows.
 * 
 * Copyright (c) 2025 Spectral Audio Systems
 ******************************************************************************/

#include "CDPSpectralEngine.h"
#include "RealtimeMemoryManager.h"
#include "PerformanceProfiler.h"
#include <random>
#include <algorithm>
#include <cmath>

//==============================================================================
// Constructor & Destructor
//==============================================================================

CDPSpectralEngine::CDPSpectralEngine()
    : currentWindowType(juce::dsp::WindowingFunction<float>::hann)
{
    // Initialize FFT with default size
    int fftSize = currentFFTSize.load();
    forwardFFT = std::make_unique<juce::dsp::FFT>(std::log2(fftSize));
    inverseFFT = std::make_unique<juce::dsp::FFT>(std::log2(fftSize));
    windowFunction = std::make_unique<juce::dsp::WindowingFunction<float>>(fftSize, currentWindowType);
    
    // Initialize phase vocoder
    phaseVocoder = std::make_unique<PhaseVocoder>();
    
    // Initialize processing buffers
    fftBuffer.resize(fftSize * 2, std::complex<float>(0.0f, 0.0f));
    windowedInput.resize(fftSize, 0.0f);
    overlapBuffer.resize(fftSize, 0.0f);
    outputBuffer.resize(fftSize, 0.0f);
    
    // Initialize spectral data storage
    int spectrumSize = fftSize / 2;
    currentMagnitudes.resize(spectrumSize, 0.0f);
    currentPhases.resize(spectrumSize, 0.0f);
    processedMagnitudes.resize(spectrumSize, 0.0f);
    processedPhases.resize(spectrumSize, 0.0f);
    
    // Initialize spectral history for temporal effects
    spectralHistory.resize(SPECTRAL_HISTORY_SIZE);
    frozenSpectrum.resize(SPECTRAL_HISTORY_SIZE);
    for (auto& frame : spectralHistory)
        frame.resize(spectrumSize, 0.0f);
    for (auto& frame : frozenSpectrum)
        frame.resize(spectrumSize, 0.0f);
    
    // Initialize phase vocoder
    phaseVocoder->analysisWindow.resize(fftSize, 0.0f);
    phaseVocoder->synthesisWindow.resize(fftSize, 0.0f);
    phaseVocoder->previousPhases.resize(spectrumSize, 0.0f);
    phaseVocoder->phaseAdvances.resize(spectrumSize, 0.0f);
    
    // Initialize effect parameters
    for (auto& param : effectParameters)
        param.store(0.0f);
    
    // Initialize effect layers
    for (auto& layer : effectLayers)
        layer = EffectLayer{};
    
    // Initialize parameter smoothers
    for (auto& smoother : parameterSmoothers)
    {
        smoother.currentValue = 0.0f;
        smoother.targetValue = 0.0f;
        smoother.smoothingFactor = 0.1f;
    }
    
    // Initialize spectral frame history
    for (auto& frame : spectralFrameHistory)
    {
        frame.magnitudes.resize(spectrumSize, 0.0f);
        frame.phases.resize(spectrumSize, 0.0f);
        frame.processedMags.resize(spectrumSize, 0.0f);
    }
    
    // Initialize performance profiler
    performanceProfiler = std::make_unique<PerformanceProfiler>();
    
    DBG("🎨 CDPSpectralEngine initialized with FFT size: " << fftSize);
}

CDPSpectralEngine::~CDPSpectralEngine()
{
    releaseResources();
}

//==============================================================================
// Audio Processing Lifecycle
//==============================================================================

void CDPSpectralEngine::prepareToPlay(double sampleRate, int samplesPerBlock, int numChannels)
{
    currentSampleRate = sampleRate;
    currentSamplesPerBlock = samplesPerBlock;
    currentNumChannels = numChannels;
    
    // Reinitialize FFT if needed
    int fftSize = currentFFTSize.load();
    if (!forwardFFT || forwardFFT->getSize() != fftSize)
    {
        forwardFFT = std::make_unique<juce::dsp::FFT>(std::log2(fftSize));
        inverseFFT = std::make_unique<juce::dsp::FFT>(std::log2(fftSize));
        windowFunction = std::make_unique<juce::dsp::WindowingFunction<float>>(fftSize, currentWindowType);
        
        // Resize buffers
        fftBuffer.resize(fftSize * 2);
        windowedInput.resize(fftSize);
        overlapBuffer.resize(fftSize);
        outputBuffer.resize(fftSize);
        
        int spectrumSize = fftSize / 2;
        currentMagnitudes.resize(spectrumSize);
        currentPhases.resize(spectrumSize);
        processedMagnitudes.resize(spectrumSize);
        processedPhases.resize(spectrumSize);
        
        // Resize spectral history
        for (auto& frame : spectralHistory)
            frame.resize(spectrumSize);
        for (auto& frame : frozenSpectrum)
            frame.resize(spectrumSize);
        for (auto& frame : spectralFrameHistory)
        {
            frame.magnitudes.resize(spectrumSize);
            frame.phases.resize(spectrumSize);
            frame.processedMags.resize(spectrumSize);
        }
    }
    
    // Initialize parameter smoothing time constants
    for (auto& smoother : parameterSmoothers)
        smoother.setSmoothingTime(10.0f, sampleRate); // 10ms smoothing
    
    // Start processing thread
    shouldStopProcessing.store(false);
    processingThread = std::make_unique<std::thread>(&CDPSpectralEngine::processingThreadFunction, this);
    processingStats.isProcessingThreadActive.store(true);
    
    DBG("🎨 CDPSpectralEngine prepared: " << sampleRate << "Hz, " << samplesPerBlock << " samples, " << numChannels << " channels");
}

void CDPSpectralEngine::processBlock(juce::AudioBuffer<float>& buffer)
{
    // REAL-TIME SAFE: Use lock-free profiler with zero allocation
    RT_SCOPED_TIMER("CDPSpectralEngine::processBlock");
    
    if (activeEffect.load() == SpectralEffect::None && activeLayerCount.load() == 0)
    {
        // No effects active, pass through
        return;
    }
    
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    const int fftSize = currentFFTSize.load();
    const float overlapFactor = currentOverlapFactor.load();
    const int hopSize = static_cast<int>(fftSize * (1.0f - overlapFactor));
    
    // Process each channel independently
    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);
        
        // Process in overlapping blocks
        for (int sampleIndex = 0; sampleIndex < numSamples; sampleIndex += hopSize)
        {
            int samplesToProcess = std::min(hopSize, numSamples - sampleIndex);
            
            // Copy input to windowed buffer
            std::fill(windowedInput.begin(), windowedInput.end(), 0.0f);
            for (int i = 0; i < samplesToProcess && i < fftSize; ++i)
            {
                if (sampleIndex + i < numSamples)
                    windowedInput[i] = channelData[sampleIndex + i];
            }
            
            // Apply window function
            windowFunction->multiplyWithWindowingTable(windowedInput.data(), fftSize);
            
            // Forward FFT
            std::fill(fftBuffer.begin(), fftBuffer.end(), std::complex<float>(0.0f, 0.0f));
            for (int i = 0; i < fftSize; ++i)
                fftBuffer[i] = std::complex<float>(windowedInput[i], 0.0f);
            
            forwardFFT->perform(fftBuffer.data(), reinterpret_cast<std::complex<float>*>(fftBuffer.data()), false);
            
            // Extract magnitude and phase
            const int spectrumSize = fftSize / 2;
            for (int i = 0; i < spectrumSize; ++i)
            {
                currentMagnitudes[i] = std::abs(fftBuffer[i]);
                currentPhases[i] = std::arg(fftBuffer[i]);
            }
            
            // Copy to processed arrays
            processedMagnitudes = currentMagnitudes;
            processedPhases = currentPhases;
            
            // Apply active spectral effects
            applyActiveSpectralEffects();
            
            // Reconstruct complex spectrum
            for (int i = 0; i < spectrumSize; ++i)
            {
                float mag = processedMagnitudes[i];
                float phase = processedPhases[i];
                fftBuffer[i] = std::complex<float>(mag * std::cos(phase), mag * std::sin(phase));
                // Mirror for real signal
                if (i > 0 && i < spectrumSize - 1)
                    fftBuffer[fftSize - i] = std::conj(fftBuffer[i]);
            }
            
            // Inverse FFT
            inverseFFT->perform(fftBuffer.data(), reinterpret_cast<std::complex<float>*>(fftBuffer.data()), true);
            
            // Extract real part and apply window
            for (int i = 0; i < fftSize; ++i)
                outputBuffer[i] = fftBuffer[i].real();
            
            windowFunction->multiplyWithWindowingTable(outputBuffer.data(), fftSize);
            
            // Overlap-add to output
            for (int i = 0; i < samplesToProcess && i < fftSize; ++i)
            {
                if (sampleIndex + i < numSamples)
                {
                    channelData[sampleIndex + i] = outputBuffer[i] + overlapBuffer[i];
                }
            }
            
            // Store overlap for next frame
            std::copy(outputBuffer.begin() + hopSize, outputBuffer.end(), overlapBuffer.begin());
            std::fill(overlapBuffer.begin() + (fftSize - hopSize), overlapBuffer.end(), 0.0f);
        }
    }
    
    // Update performance statistics
    updateProcessingStats();
    
    // Store spectral frame for visualization
    if (spectralVisualizationEnabled.load())
        storeSpectralFrame();
}

void CDPSpectralEngine::releaseResources()
{
    // Stop processing thread
    if (processingThread && processingThread->joinable())
    {
        shouldStopProcessing.store(true);
        processingThread->join();
        processingThread.reset();
    }
    
    processingStats.isProcessingThreadActive.store(false);
    
    DBG("🎨 CDPSpectralEngine resources released");
}

//==============================================================================
// CDP-Style Spectral Effects Control
//==============================================================================

void CDPSpectralEngine::setSpectralEffect(SpectralEffect effect, float intensity)
{
    activeEffect.store(effect);
    effectIntensity.store(juce::jlimit(0.0f, 1.0f, intensity));
    
    // Update visual feedback state for MetaSynth-style canvas
    currentEffect.store(effect);
    currentIntensity.store(juce::jlimit(0.0f, 1.0f, intensity));
    
    // Push command to processing thread
    ProcessingCommand cmd;
    cmd.type = ProcessingCommand::SetEffect;
    cmd.effect = effect;
    cmd.value = intensity;
    pushCommand(cmd);
    
    DBG("🎨 Spectral Effect Set: " << static_cast<int>(effect) << " intensity: " << intensity);
}

void CDPSpectralEngine::setEffectParameter(SpectralEffect effect, int paramIndex, float value)
{
    if (paramIndex >= 0 && paramIndex < effectParameters.size())
    {
        effectParameters[paramIndex].store(value);
        
        // Push command to processing thread
        ProcessingCommand cmd;
        cmd.type = ProcessingCommand::SetParameter;
        cmd.effect = effect;
        cmd.paramIndex = paramIndex;
        cmd.value = value;
        pushCommand(cmd);
    }
}

float CDPSpectralEngine::getEffectParameter(SpectralEffect effect, int paramIndex) const
{
    if (paramIndex >= 0 && paramIndex < effectParameters.size())
        return effectParameters[paramIndex].load();
    return 0.0f;
}

void CDPSpectralEngine::addSpectralLayer(SpectralEffect effect, float intensity, float mix)
{
    // Find available layer slot
    for (int i = 0; i < MAX_EFFECT_LAYERS; ++i)
    {
        if (!effectLayers[i].active)
        {
            effectLayers[i].effect = effect;
            effectLayers[i].intensity = juce::jlimit(0.0f, 1.0f, intensity);
            effectLayers[i].mix = juce::jlimit(0.0f, 1.0f, mix);
            effectLayers[i].active = true;
            
            activeLayerCount.store(activeLayerCount.load() + 1);
            
            DBG("🎨 Added Spectral Layer: " << static_cast<int>(effect) 
                << " intensity: " << intensity << " mix: " << mix);
            break;
        }
    }
}

void CDPSpectralEngine::clearSpectralLayers()
{
    for (auto& layer : effectLayers)
        layer.active = false;
    
    activeLayerCount.store(0);
    
    DBG("🎨 Cleared all spectral layers");
}

int CDPSpectralEngine::getActiveLayerCount() const
{
    return activeLayerCount.load();
}

//==============================================================================
// Paint-to-Spectral Parameter Mapping
//==============================================================================

void CDPSpectralEngine::processPaintSpectralData(const PaintSpectralData& paintData)
{
    // Push command to processing thread
    ProcessingCommand cmd;
    cmd.type = ProcessingCommand::SetPaintData;
    cmd.paintData = paintData;
    pushCommand(cmd);
}

void CDPSpectralEngine::updateSpectralParameters(const PaintSpectralData& paintData)
{
    // Map hue to spectral effect type
    SpectralEffect mappedEffect = hueToSpectralEffect(paintData.hue);
    
    // Set primary effect based on paint data
    setSpectralEffect(mappedEffect, paintData.saturation);
    
    // Map additional parameters based on paint data
    float smoothedPressure = parameterSmoothers[0].getNext();
    parameterSmoothers[0].setTarget(paintData.pressure);
    
    float smoothedVelocity = parameterSmoothers[1].getNext();
    parameterSmoothers[1].setTarget(paintData.velocity);
    
    // Effect-specific parameter mapping
    switch (mappedEffect)
    {
        case SpectralEffect::Blur:
            setEffectParameter(mappedEffect, 0, paintData.positionX);      // Blur kernel size
            setEffectParameter(mappedEffect, 1, paintData.positionY);      // Blur direction bias
            break;
            
        case SpectralEffect::Randomize:
            setEffectParameter(mappedEffect, 0, smoothedVelocity);         // Randomization rate
            setEffectParameter(mappedEffect, 1, paintData.brightness);     // Amplitude randomization
            break;
            
        case SpectralEffect::Shuffle:
            setEffectParameter(mappedEffect, 0, paintData.positionX);      // Shuffle pattern
            setEffectParameter(mappedEffect, 1, smoothedPressure);         // Shuffle intensity
            break;
            
        case SpectralEffect::Freeze:
            setEffectParameter(mappedEffect, 0, paintData.positionY);      // Frequency band selection
            setEffectParameter(mappedEffect, 1, paintData.brightness);     // Freeze decay time
            break;
            
        case SpectralEffect::Arpeggiate:
            setEffectParameter(mappedEffect, 0, smoothedVelocity * 4.0f);  // Arpeggiate rate
            setEffectParameter(mappedEffect, 1, paintData.positionY);      // Direction (up/down)
            break;
            
        case SpectralEffect::TimeExpand:
            setEffectParameter(mappedEffect, 0, 0.5f + paintData.positionX); // Time stretch factor
            setEffectParameter(mappedEffect, 1, paintData.brightness);      // Formant preservation
            break;
            
        default:
            break;
    }
}

//==============================================================================
// Threading System
//==============================================================================

void CDPSpectralEngine::processingThreadFunction()
{
    juce::Thread::setCurrentThreadName("CDPSpectralEngine");
    
    while (!shouldStopProcessing.load())
    {
        ProcessingCommand cmd;
        while (popCommand(cmd))
        {
            switch (cmd.type)
            {
                case ProcessingCommand::SetEffect:
                    // Handle effect changes
                    break;
                    
                case ProcessingCommand::SetParameter:
                    // Handle parameter changes
                    break;
                    
                case ProcessingCommand::SetPaintData:
                    updateSpectralParameters(cmd.paintData);
                    break;
                    
                case ProcessingCommand::SetTempo:
                    hostTempo.store(cmd.tempoInfo);
                    break;
            }
        }
        
        // Adaptive processing updates
        updateAdaptiveProcessing();
        
        // Sleep briefly to avoid consuming too much CPU
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
}

void CDPSpectralEngine::pushCommand(const ProcessingCommand& command)
{
    int writeIndex = commandQueueWriteIndex.load();
    int nextIndex = (writeIndex + 1) % COMMAND_QUEUE_SIZE;
    
    if (nextIndex != commandQueueReadIndex.load())
    {
        commandQueue[writeIndex] = command;
        commandQueueWriteIndex.store(nextIndex);
    }
    // If queue is full, command is dropped (could log this for debugging)
}

bool CDPSpectralEngine::popCommand(ProcessingCommand& command)
{
    int readIndex = commandQueueReadIndex.load();
    if (readIndex == commandQueueWriteIndex.load())
        return false; // Queue is empty
    
    command = commandQueue[readIndex];
    commandQueueReadIndex.store((readIndex + 1) % COMMAND_QUEUE_SIZE);
    return true;
}

//==============================================================================
// Individual Spectral Effects Implementation
//==============================================================================

void CDPSpectralEngine::applyActiveSpectralEffects()
{
    SpectralEffect primaryEffect = activeEffect.load();
    float intensity = effectIntensity.load();
    
    if (primaryEffect != SpectralEffect::None && intensity > 0.0f)
    {
        // Apply primary effect
        switch (primaryEffect)
        {
            case SpectralEffect::Blur:
                applySpectralBlur(processedMagnitudes, intensity);
                break;
                
            case SpectralEffect::Randomize:
                applySpectralRandomize(processedMagnitudes, processedPhases, intensity);
                break;
                
            case SpectralEffect::Shuffle:
                applySpectralShuffle(processedMagnitudes, processedPhases, intensity);
                break;
                
            case SpectralEffect::Freeze:
                applySpectralFreeze(processedMagnitudes, processedPhases, intensity);
                break;
                
            case SpectralEffect::Arpeggiate:
                {
                    float rate = effectParameters[0].load();
                    applySpectralArpeggiate(processedMagnitudes, processedPhases, rate, intensity);
                }
                break;
                
            case SpectralEffect::TimeExpand:
                {
                    float factor = effectParameters[0].load();
                    applySpectralTimeExpand(processedMagnitudes, processedPhases, factor);
                }
                break;
                
            default:
                break;
        }
    }
    
    // Apply layered effects
    int layerCount = activeLayerCount.load();
    for (int i = 0; i < layerCount && i < MAX_EFFECT_LAYERS; ++i)
    {
        if (effectLayers[i].active && effectLayers[i].intensity > 0.0f)
        {
            // Store original state
            auto originalMags = processedMagnitudes;
            auto originalPhases = processedPhases;
            
            // Apply layer effect
            float layerIntensity = effectLayers[i].intensity;
            switch (effectLayers[i].effect)
            {
                case SpectralEffect::Blur:
                    applySpectralBlur(processedMagnitudes, layerIntensity);
                    break;
                    
                case SpectralEffect::Randomize:
                    applySpectralRandomize(processedMagnitudes, processedPhases, layerIntensity);
                    break;
                    
                // Add other effects as needed
                default:
                    break;
            }
            
            // Mix with original based on layer mix amount
            float mix = effectLayers[i].mix;
            for (size_t j = 0; j < processedMagnitudes.size(); ++j)
            {
                processedMagnitudes[j] = originalMags[j] * (1.0f - mix) + processedMagnitudes[j] * mix;
                processedPhases[j] = originalPhases[j] * (1.0f - mix) + processedPhases[j] * mix;
            }
        }
    }
}

void CDPSpectralEngine::applySpectralBlur(std::vector<float>& magnitudes, float intensity)
{
    if (intensity <= 0.0f) return;
    
    // CDP-style spectral blur using convolution with gaussian-like kernel
    float kernelSize = 1.0f + intensity * 8.0f; // 1-9 bin kernel
    int kernelRadius = static_cast<int>(kernelSize);
    
    std::vector<float> blurred(magnitudes.size(), 0.0f);
    
    for (size_t i = 0; i < magnitudes.size(); ++i)
    {
        float sum = 0.0f;
        float weightSum = 0.0f;
        
        for (int k = -kernelRadius; k <= kernelRadius; ++k)
        {
            int index = static_cast<int>(i) + k;
            if (index >= 0 && index < static_cast<int>(magnitudes.size()))
            {
                float weight = std::exp(-0.5f * (k * k) / (kernelSize * kernelSize));
                sum += magnitudes[index] * weight;
                weightSum += weight;
            }
        }
        
        if (weightSum > 0.0f)
            blurred[i] = sum / weightSum;
    }
    
    // Mix with original based on intensity
    for (size_t i = 0; i < magnitudes.size(); ++i)
    {
        magnitudes[i] = magnitudes[i] * (1.0f - intensity) + blurred[i] * intensity;
    }
}

void CDPSpectralEngine::applySpectralRandomize(std::vector<float>& magnitudes, std::vector<float>& phases, float intensity)
{
    if (intensity <= 0.0f) return;
    
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(-1.0f, 1.0f);
    
    // Randomize phases (CDP-style spectral scrambling)
    for (size_t i = 0; i < phases.size(); ++i)
    {
        float randomPhase = dis(gen) * juce::MathConstants<float>::pi;
        phases[i] = phases[i] * (1.0f - intensity) + randomPhase * intensity;
    }
    
    // Subtle magnitude randomization
    float magRandomization = effectParameters[1].load() * intensity;
    if (magRandomization > 0.0f)
    {
        for (size_t i = 0; i < magnitudes.size(); ++i)
        {
            float randomFactor = 1.0f + dis(gen) * magRandomization * 0.2f;
            magnitudes[i] *= randomFactor;
        }
    }
}

void CDPSpectralEngine::applySpectralShuffle(std::vector<float>& magnitudes, std::vector<float>& phases, float intensity)
{
    if (intensity <= 0.0f) return;
    
    // CDP-style frequency bin shuffling
    static std::random_device rd;
    static std::mt19937 gen(rd());
    
    // Create shuffled indices
    std::vector<int> indices(magnitudes.size());
    std::iota(indices.begin(), indices.end(), 0);
    
    // Shuffle with intensity control
    int shuffleAmount = static_cast<int>(intensity * magnitudes.size() * 0.5f);
    for (int i = 0; i < shuffleAmount; ++i)
    {
        std::uniform_int_distribution<int> indexDis(0, static_cast<int>(magnitudes.size()) - 1);
        int idx1 = indexDis(gen);
        int idx2 = indexDis(gen);
        std::swap(indices[idx1], indices[idx2]);
    }
    
    // Apply shuffle
    std::vector<float> shuffledMags(magnitudes.size());
    std::vector<float> shuffledPhases(phases.size());
    
    for (size_t i = 0; i < magnitudes.size(); ++i)
    {
        shuffledMags[i] = magnitudes[indices[i]];
        shuffledPhases[i] = phases[indices[i]];
    }
    
    // Mix with original
    for (size_t i = 0; i < magnitudes.size(); ++i)
    {
        magnitudes[i] = magnitudes[i] * (1.0f - intensity) + shuffledMags[i] * intensity;
        phases[i] = phases[i] * (1.0f - intensity) + shuffledPhases[i] * intensity;
    }
}

void CDPSpectralEngine::applySpectralFreeze(std::vector<float>& magnitudes, std::vector<float>& phases, float intensity)
{
    if (intensity <= 0.0f) return;
    
    // CDP-style spectral freeze - sustain certain frequency bands
    float bandSelection = effectParameters[0].load(); // Which bands to freeze (0-1)
    
    // Determine frequency range to freeze
    int startBin = static_cast<int>(bandSelection * magnitudes.size() * 0.8f);
    int endBin = std::min(static_cast<int>((bandSelection + 0.2f) * magnitudes.size()), 
                         static_cast<int>(magnitudes.size()));
    
    // Store frozen spectrum on first frame or update
    static bool freezeUpdated = false;
    if (!freezeUpdated || intensity > 0.9f) // Update frozen spectrum when intensity is high
    {
        if (!frozenSpectrum.empty() && !frozenSpectrum[0].empty())
        {
            for (int i = startBin; i < endBin; ++i)
            {
                if (i < static_cast<int>(frozenSpectrum[0].size()))
                    frozenSpectrum[0][i] = magnitudes[i];
            }
        }
        freezeUpdated = true;
    }
    
    // Apply frozen spectrum
    if (!frozenSpectrum.empty() && !frozenSpectrum[0].empty())
    {
        for (int i = startBin; i < endBin; ++i)
        {
            if (i < static_cast<int>(magnitudes.size()) && i < static_cast<int>(frozenSpectrum[0].size()))
            {
                magnitudes[i] = magnitudes[i] * (1.0f - intensity) + frozenSpectrum[0][i] * intensity;
            }
        }
    }
}

void CDPSpectralEngine::applySpectralArpeggiate(std::vector<float>& magnitudes, std::vector<float>& phases, float rate, float intensity)
{
    if (intensity <= 0.0f) return;
    
    // CDP-style spectral arpeggiation - temporal sequencing of frequency bands
    static int arpeggiateCounter = 0;
    arpeggiateCounter++;
    
    // Calculate arpeggiate step based on rate and tempo
    double tempo = hostTempo.load();
    double samplesPerBeat = currentSampleRate * 60.0 / tempo;
    int stepSize = static_cast<int>(samplesPerBeat / (rate * 4.0f)); // Rate in beats
    
    if (stepSize <= 0) stepSize = 1;
    
    int currentStep = (arpeggiateCounter / stepSize) % 8; // 8-step arpeggiator
    
    // Apply arpeggiation by emphasizing certain frequency bands
    int bandsPerStep = static_cast<int>(magnitudes.size()) / 8;
    int activeBandStart = currentStep * bandsPerStep;
    int activeBandEnd = (currentStep + 1) * bandsPerStep;
    
    // Direction control from parameter
    float direction = effectParameters[1].load(); // 0 = up, 1 = down
    if (direction > 0.5f)
    {
        activeBandStart = magnitudes.size() - activeBandEnd;
        activeBandEnd = magnitudes.size() - (currentStep * bandsPerStep);
    }
    
    // Apply arpeggiation effect
    for (size_t i = 0; i < magnitudes.size(); ++i)
    {
        float gain = 1.0f;
        if (static_cast<int>(i) >= activeBandStart && static_cast<int>(i) < activeBandEnd)
        {
            gain = 1.0f + intensity; // Emphasize active band
        }
        else
        {
            gain = 1.0f - intensity * 0.7f; // Reduce other bands
        }
        
        magnitudes[i] *= gain;
    }
}

void CDPSpectralEngine::applySpectralTimeExpand(std::vector<float>& magnitudes, std::vector<float>& phases, float factor)
{
    // CDP-style time expansion using phase vocoder
    if (factor <= 0.0f) factor = 1.0f;
    
    phaseVocoder->timeStretchRatio = factor;
    
    // Update phase advances for time stretching
    int spectrumSize = static_cast<int>(phases.size());
    float fundamental = 2.0f * juce::MathConstants<float>::pi * currentFFTSize.load() / (4.0f * currentSampleRate);
    
    for (int i = 0; i < spectrumSize; ++i)
    {
        float expectedPhaseAdvance = fundamental * i;
        float phaseDeviation = phases[i] - phaseVocoder->previousPhases[i] - expectedPhaseAdvance;
        
        // Wrap phase deviation to [-π, π]
        while (phaseDeviation > juce::MathConstants<float>::pi) 
            phaseDeviation -= 2.0f * juce::MathConstants<float>::pi;
        while (phaseDeviation < -juce::MathConstants<float>::pi) 
            phaseDeviation += 2.0f * juce::MathConstants<float>::pi;
        
        // Calculate true frequency
        float trueFreq = expectedPhaseAdvance + phaseDeviation;
        
        // Update phase for time stretching
        phaseVocoder->phaseAdvances[i] += trueFreq / factor;
        phases[i] = phaseVocoder->phaseAdvances[i];
        
        // Store current phase for next frame
        phaseVocoder->previousPhases[i] = phases[i];
    }
}

void CDPSpectralEngine::applySpectralAverage(std::vector<float>& magnitudes, int windowSize)
{
    // CDP-style spectral averaging across time
    if (windowSize <= 1 || spectralHistory.empty()) return;
    
    // Add current frame to history
    static int historyIndex = 0;
    if (historyIndex < static_cast<int>(spectralHistory.size()))
    {
        spectralHistory[historyIndex] = magnitudes;
        historyIndex = (historyIndex + 1) % spectralHistory.size();
    }
    
    // Calculate average
    std::vector<float> averaged(magnitudes.size(), 0.0f);
    int framesToAverage = std::min(windowSize, static_cast<int>(spectralHistory.size()));
    
    for (int frame = 0; frame < framesToAverage; ++frame)
    {
        if (frame < static_cast<int>(spectralHistory.size()))
        {
            for (size_t bin = 0; bin < magnitudes.size() && bin < spectralHistory[frame].size(); ++bin)
            {
                averaged[bin] += spectralHistory[frame][bin];
            }
        }
    }
    
    // Normalize and apply
    if (framesToAverage > 0)
    {
        for (size_t i = 0; i < magnitudes.size(); ++i)
        {
            magnitudes[i] = averaged[i] / static_cast<float>(framesToAverage);
        }
    }
}

void CDPSpectralEngine::applySpectralMorph(std::vector<float>& magnitudes, const std::vector<float>& targetMags, float amount)
{
    // CDP-style spectral morphing between two spectra
    if (amount <= 0.0f || targetMags.size() != magnitudes.size()) return;
    
    for (size_t i = 0; i < magnitudes.size(); ++i)
    {
        magnitudes[i] = magnitudes[i] * (1.0f - amount) + targetMags[i] * amount;
    }
}

//==============================================================================
// Utility Methods
//==============================================================================

CDPSpectralEngine::SpectralEffect CDPSpectralEngine::hueToSpectralEffect(float hue) const
{
    // Map hue (0-1) to spectral effects
    if (hue < 0.16f)        return SpectralEffect::Blur;        // Red
    else if (hue < 0.33f)   return SpectralEffect::Arpeggiate;  // Green  
    else if (hue < 0.5f)    return SpectralEffect::Freeze;      // Blue
    else if (hue < 0.66f)   return SpectralEffect::Randomize;   // Yellow
    else if (hue < 0.83f)   return SpectralEffect::TimeExpand;  // Magenta
    else                    return SpectralEffect::Average;     // Cyan
}

void CDPSpectralEngine::updateAdaptiveProcessing()
{
    if (currentProcessingMode.load() == ProcessingMode::Adaptive)
    {
        // Monitor CPU usage and adjust processing parameters
        float cpuUsage = processingStats.cpuUsage.load();
        
        if (cpuUsage > 0.8f) // High CPU usage
        {
            // Reduce quality for performance
            if (currentFFTSize.load() > 512)
                setFFTSize(currentFFTSize.load() / 2);
            if (currentOverlapFactor.load() > 0.5f)
                setOverlapFactor(currentOverlapFactor.load() - 0.125f);
        }
        else if (cpuUsage < 0.4f) // Low CPU usage
        {
            // Increase quality if we have headroom
            if (currentFFTSize.load() < 2048)
                setFFTSize(currentFFTSize.load() * 2);
            if (currentOverlapFactor.load() < 0.75f)
                setOverlapFactor(currentOverlapFactor.load() + 0.125f);
        }
    }
}

void CDPSpectralEngine::updateProcessingStats()
{
    auto currentTime = std::chrono::high_resolution_clock::now();
    if (lastProcessTime.time_since_epoch().count() > 0)
    {
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - lastProcessTime);
        float latencyMs = duration.count() / 1000.0f;
        processingStats.latencyMs.store(latencyMs);
        
        // Update CPU usage estimate (simplified)
        float targetLatency = latencyTargetMs.load();
        float cpuEstimate = std::min(1.0f, latencyMs / targetLatency);
        processingStats.cpuUsage.store(cpuEstimate);
    }
    lastProcessTime = currentTime;
    
    // Update other stats
    processingStats.activeEffects.store((activeEffect.load() != SpectralEffect::None) ? 1 : 0 + activeLayerCount.load());
}

void CDPSpectralEngine::storeSpectralFrame()
{
    int index = spectralFrameIndex.load();
    
    SpectralFrame& frame = spectralFrameHistory[index];
    frame.magnitudes = currentMagnitudes;
    frame.phases = currentPhases;
    frame.processedMags = processedMagnitudes;
    frame.timestamp = std::chrono::high_resolution_clock::now();
    
    // Calculate spectral features
    if (!currentMagnitudes.empty())
    {
        // Spectral centroid
        float weightedSum = 0.0f;
        float magnitudeSum = 0.0f;
        for (size_t i = 0; i < currentMagnitudes.size(); ++i)
        {
            weightedSum += i * currentMagnitudes[i];
            magnitudeSum += currentMagnitudes[i];
        }
        frame.spectralCentroid = magnitudeSum > 0.0f ? weightedSum / magnitudeSum : 0.0f;
        
        // Spectral spread
        float variance = 0.0f;
        for (size_t i = 0; i < currentMagnitudes.size(); ++i)
        {
            float deviation = i - frame.spectralCentroid;
            variance += deviation * deviation * currentMagnitudes[i];
        }
        frame.spectralSpread = magnitudeSum > 0.0f ? std::sqrt(variance / magnitudeSum) : 0.0f;
        
        // Spectral entropy (simplified)
        float entropy = 0.0f;
        for (size_t i = 0; i < currentMagnitudes.size(); ++i)
        {
            if (currentMagnitudes[i] > 0.0f)
            {
                float probability = currentMagnitudes[i] / magnitudeSum;
                entropy -= probability * std::log2(probability);
            }
        }
        frame.spectralEntropy = entropy;
    }
    
    spectralFrameIndex.store((index + 1) % SPECTRAL_HISTORY_SIZE);
}

//==============================================================================
// Configuration Methods
//==============================================================================

void CDPSpectralEngine::setProcessingMode(ProcessingMode mode)
{
    currentProcessingMode.store(mode);
    
    // Apply mode-specific settings
    switch (mode)
    {
        case ProcessingMode::RealTime:
            setFFTSize(512);
            setOverlapFactor(0.5f);
            break;
            
        case ProcessingMode::Quality:
            setFFTSize(2048);
            setOverlapFactor(0.75f);
            break;
            
        case ProcessingMode::Adaptive:
            // Will be handled in updateAdaptiveProcessing()
            break;
    }
    
    DBG("🎨 Processing mode set to: " << static_cast<int>(mode));
}

void CDPSpectralEngine::setFFTSize(int fftSize)
{
    // Validate FFT size (must be power of 2)
    int validSize = 1;
    while (validSize < fftSize && validSize < 8192)
        validSize *= 2;
    
    if (validSize >= 512 && validSize <= 4096)
    {
        currentFFTSize.store(validSize);
        DBG("🎨 FFT size set to: " << validSize);
    }
}

void CDPSpectralEngine::setOverlapFactor(float overlap)
{
    float validOverlap = juce::jlimit(0.25f, 0.875f, overlap);
    currentOverlapFactor.store(validOverlap);
    DBG("🎨 Overlap factor set to: " << validOverlap);
}

void CDPSpectralEngine::setWindowType(juce::dsp::WindowingFunction<float>::WindowingMethod windowType)
{
    currentWindowType = windowType;
    windowFunction = std::make_unique<juce::dsp::WindowingFunction<float>>(currentFFTSize.load(), windowType);
    DBG("🎨 Window type set to: " << static_cast<int>(windowType));
}

//==============================================================================
// Tempo Synchronization
//==============================================================================

void CDPSpectralEngine::setHostTempo(double bpm)
{
    ProcessingCommand cmd;
    cmd.type = ProcessingCommand::SetTempo;
    cmd.tempoInfo = bpm;
    pushCommand(cmd);
}

void CDPSpectralEngine::setHostTimeSignature(int numerator, int denominator)
{
    hostTimeSignatureNum.store(numerator);
    hostTimeSignatureDen.store(denominator);
}

void CDPSpectralEngine::setHostTransportPlaying(bool isPlaying)
{
    hostIsPlaying.store(isPlaying);
}

void CDPSpectralEngine::setHostPPQPosition(double ppqPosition)
{
    hostPPQPosition.store(ppqPosition);
}

//==============================================================================
// Spectral Analysis Access
//==============================================================================

CDPSpectralEngine::SpectralFrame CDPSpectralEngine::getCurrentSpectralFrame() const
{
    int currentIndex = (spectralFrameIndex.load() - 1 + SPECTRAL_HISTORY_SIZE) % SPECTRAL_HISTORY_SIZE;
    return spectralFrameHistory[currentIndex];
}

std::vector<CDPSpectralEngine::SpectralFrame> CDPSpectralEngine::getRecentSpectralHistory(int numFrames) const
{
    std::vector<SpectralFrame> history;
    int requestedFrames = std::min(numFrames, SPECTRAL_HISTORY_SIZE);
    
    for (int i = 0; i < requestedFrames; ++i)
    {
        int index = (spectralFrameIndex.load() - 1 - i + SPECTRAL_HISTORY_SIZE) % SPECTRAL_HISTORY_SIZE;
        history.push_back(spectralFrameHistory[index]);
    }
    
    return history;
}

//==============================================================================
// Performance Control
//==============================================================================

void CDPSpectralEngine::resetProcessingStats()
{
    processingStats.cpuUsage.store(0.0f);
    processingStats.activeEffects.store(0);
    processingStats.latencyMs.store(0.0f);
    processingStats.bufferUnderruns.store(0);
    processingStats.spectralComplexity.store(0.0f);
    processingStats.frozenBands.store(0);
    processingStats.morphAmount.store(0.0f);
}

void CDPSpectralEngine::enablePerformanceMode(bool enable)
{
    if (enable)
    {
        setProcessingMode(ProcessingMode::RealTime);
        maxConcurrentEffects = 2;
    }
    else
    {
        setProcessingMode(ProcessingMode::Quality);
        maxConcurrentEffects = 8;
    }
}

//==============================================================================
// Preset System
//==============================================================================

void CDPSpectralEngine::loadSpectralPreset(const SpectralPreset& preset)
{
    // Clear current effects
    clearSpectralLayers();
    
    // Set primary effect
    setSpectralEffect(preset.primaryEffect, 1.0f);
    
    // Add layered effects
    for (const auto& layeredEffect : preset.layeredEffects)
    {
        addSpectralLayer(layeredEffect.first, layeredEffect.second);
    }
    
    // Apply parameters
    for (const auto& param : preset.parameters)
    {
        // Parse parameter name and set value
        // This would need more sophisticated parameter mapping
    }
    
    // Set recommended processing mode
    setProcessingMode(preset.recommendedMode);
    
    DBG("🎨 Loaded spectral preset: " << preset.name);
}

CDPSpectralEngine::SpectralPreset CDPSpectralEngine::getCurrentPreset() const
{
    SpectralPreset preset;
    preset.name = "Current";
    preset.primaryEffect = activeEffect.load();
    preset.recommendedMode = currentProcessingMode.load();
    
    // Add active layers
    for (const auto& layer : effectLayers)
    {
        if (layer.active)
        {
            preset.layeredEffects.emplace_back(layer.effect, layer.intensity);
        }
    }
    
    return preset;
}

void CDPSpectralEngine::saveCurrentAsPreset(const juce::String& name, const juce::String& description)
{
    SpectralPreset preset = getCurrentPreset();
    preset.name = name;
    preset.description = description;
    
    // Save to preset manager (would need implementation)
    DBG("🎨 Saved spectral preset: " << name);
}