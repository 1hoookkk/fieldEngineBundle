#pragma once

#include <juce_dsp/juce_dsp.h>
#include <cstdint>

/**
 * RT-safe parameter snapshot system
 * 
 * This creates a per-block POD snapshot of all parameters to ensure:
 * 1. No APVTS locks on audio thread (APVTS state ops use locks)
 * 2. Sample-accurate parameter ramping within blocks
 * 3. Stable parameter values throughout DSP processing
 */
struct ParameterSnapshot
{
    // Engine control
    uint8_t audioPath;                    // AudioPath enum (0=Silent, 1=TestFeeder, 2=Phase4Synth, 3=SpectralEngine)
    bool useTestFeeder;
    bool useFullSpectralEngine;
    
    // Audio processing parameters (require smoothing)
    float masterGain;                     // -24..+24 dB (needs ramping)
    float spectralGain;                   // 0..2 (needs ramping) 
    float spectralMix;                    // 0..1 (needs ramping)
    float blend;                          // 0..1 Synth/Resynth blend (needs ramping)
    float oscGain;                        // 0..1 Phase4 oscillator gain (needs ramping)
    float maskDepth;                      // 0..1 mask intensity (needs ramping)
    
    // DSP parameters (can step)
    uint16_t fftSize;                     // 256, 512, 1024, 2048
    uint16_t hopSize;                     // FFT_SIZE/4 typically
    // respeed removed for live insert mode
    uint8_t synthMode;                    // 0=Synth, 1=Resynth, 2=Hybrid
    
    // Z-Plane filter parameters
    float filterCutoff;                   // 20..20000 Hz
    float filterResonance;                // 0.1..10
    float filterType;                     // 0.0=LP, 0.5=BP, 1.0=HP
    float filterMorphAmount;              // 0..1
    float zPlaneMorphX;                   // 0..1
    float zPlaneMorphY;                   // 0..1
    
    // Brush system
    uint8_t brushType;                    // 0=Paint, 1=Quantize, 2=Erase, 3=Comb
    float brushSize;                      // 1..128 bins
    float brushStrength;                  // 0..1
    float quantizeStrength;               // 0..1
    
    // Tube saturation
    float saturationDrive;                // 0..10
    float saturationMix;                  // 0..1
    bool saturationOversampling;
    
    // Phase4 experimental parameters
    bool keyFilterEnabled;
    uint8_t scaleType;                    // 0=Chromatic, 1=Major, 2=Minor
    uint8_t rootNote;                     // 0-11 pitch classes
    
    // Grid and tuning
    bool gridOn;
    uint8_t tuningId;                     // 0=12TET, custom N-EDO...
    
    // Visual parameters (passed to GPU thread)
    uint8_t colormapType;                 // 0=Viridis, 1=Magma, 2=Purple_Cyan
    float particleCount;                  // 100..5000
    float animationSpeed;                 // Animation multiplier
    float visualGamma;                    // 0.5..1.5 display gamma
    float visualGain;                     // 0..4 display gain
    bool freezeDisplay;                   // Freeze/scroll toggle
    bool fastPaintMode;                   // Low-latency mode
    bool gpuAcceleration;                 // GPU enable/disable
    
    // Debug and testing
    bool testModeActive;                  // Start/Stop test toggle
    bool debugOverlayEnabled;             // Debug overlay toggle
    
    // Timing (set by processor)
    uint64_t blockStartSample;            // Sample timestamp when block began
    uint64_t engineFrameId;               // Monotonic frame counter
    
    ParameterSnapshot() noexcept
    {
        // Initialize to safe defaults
        audioPath = 0; // Silent
        useTestFeeder = false;
        useFullSpectralEngine = false;
        
        masterGain = 1.0f;
        spectralGain = 1.0f;
        spectralMix = 1.0f;
        blend = 0.0f;
        oscGain = 0.2f;
        maskDepth = 0.7f;
        
        fftSize = 512;
        hopSize = 128;
        // respeed removed for live insert mode
        synthMode = 0;
        
        filterCutoff = 1000.0f;
        filterResonance = 0.7f;
        filterType = 0.0f;
        filterMorphAmount = 0.5f;
        zPlaneMorphX = 0.5f;
        zPlaneMorphY = 0.5f;
        
        brushType = 0;
        brushSize = 16.0f;
        brushStrength = 0.7f;
        quantizeStrength = 0.5f;
        
        saturationDrive = 1.0f;
        saturationMix = 0.3f;
        saturationOversampling = false;
        
        keyFilterEnabled = false;
        scaleType = 1; // Major
        rootNote = 0;  // C
        
        gridOn = false;
        tuningId = 0;
        
        colormapType = 2; // Purple_Cyan
        particleCount = 1000.0f;
        animationSpeed = 1.0f;
        visualGamma = 1.0f;
        visualGain = 1.0f;
        freezeDisplay = false;
        fastPaintMode = false;
        gpuAcceleration = true;
        
        testModeActive = false;
        debugOverlayEnabled = false;
        
        blockStartSample = 0;
        engineFrameId = 0;
    }
};

// Ensure POD compliance for RT safety
static_assert(std::is_trivially_copyable_v<ParameterSnapshot>, 
              "ParameterSnapshot must be POD for RT safety");

/**
 * Parameter smoothing system for sample-accurate ramping
 * 
 * Uses JUCE's SmoothedValue for parameters that need ramping to avoid clicks/steps
 * Called once per block after building ParameterSnapshot
 */
class ParameterSmoother
{
public:
    ParameterSmoother() noexcept = default;
    
    void prepare(double sampleRate, int maxBlockSize) noexcept
    {
        // Configure smoothing for parameters that need ramping
        const float rampTimeMs = 10.0f; // 10ms smoothing time
        
        masterGain.setTargetValue(1.0f);
        masterGain.reset(sampleRate, rampTimeMs / 1000.0f);
        
        spectralGain.setTargetValue(1.0f);
        spectralGain.reset(sampleRate, rampTimeMs / 1000.0f);
        
        spectralMix.setTargetValue(1.0f);
        spectralMix.reset(sampleRate, rampTimeMs / 1000.0f);
        
        blend.setTargetValue(0.0f);
        blend.reset(sampleRate, rampTimeMs / 1000.0f);
        
        oscGain.setTargetValue(0.2f);
        oscGain.reset(sampleRate, rampTimeMs / 1000.0f);
        
        maskDepth.setTargetValue(0.7f);
        maskDepth.reset(sampleRate, rampTimeMs / 1000.0f);
    }
    
    void reset() noexcept
    {
        masterGain.setCurrentAndTargetValue(1.0f);
        spectralGain.setCurrentAndTargetValue(1.0f);
        spectralMix.setCurrentAndTargetValue(1.0f);
        blend.setCurrentAndTargetValue(0.0f);
        oscGain.setCurrentAndTargetValue(0.2f);
        maskDepth.setCurrentAndTargetValue(0.7f);
    }
    
    // Apply target values from ParameterSnapshot (called once per block)
    void setTargets(const ParameterSnapshot& snapshot) noexcept
    {
        // Convert dB to linear for master gain
        const float gainLinear = juce::Decibels::decibelsToGain(
            juce::jmap(snapshot.masterGain, -24.0f, 24.0f));
        
        masterGain.setTargetValue(gainLinear);
        spectralGain.setTargetValue(snapshot.spectralGain);
        spectralMix.setTargetValue(snapshot.spectralMix);
        blend.setTargetValue(snapshot.blend);
        oscGain.setTargetValue(snapshot.oscGain);
        maskDepth.setTargetValue(snapshot.maskDepth);
    }
    
    // Get smoothed values during sample processing
    float getMasterGain() noexcept { return masterGain.getNextValue(); }
    float getSpectralGain() noexcept { return spectralGain.getNextValue(); }
    float getSpectralMix() noexcept { return spectralMix.getNextValue(); }
    float getBlend() noexcept { return blend.getNextValue(); }
    float getOscGain() noexcept { return oscGain.getNextValue(); }
    float getMaskDepth() noexcept { return maskDepth.getNextValue(); }
    
    // Skip N samples (for efficiency when not all params are used)
    void skip(int numSamples) noexcept
    {
        masterGain.skip(numSamples);
        spectralGain.skip(numSamples);
        spectralMix.skip(numSamples);
        blend.skip(numSamples);
        oscGain.skip(numSamples);
        maskDepth.skip(numSamples);
    }
    
private:
    // Smoothed values for parameters requiring ramping
    juce::SmoothedValue<float> masterGain;
    juce::SmoothedValue<float> spectralGain;
    juce::SmoothedValue<float> spectralMix;
    juce::SmoothedValue<float> blend;
    juce::SmoothedValue<float> oscGain;
    juce::SmoothedValue<float> maskDepth;
};