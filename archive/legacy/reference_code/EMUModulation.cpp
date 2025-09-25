/**
 * EMU Modulation System Implementation  
 * Classic ADSR envelopes and LFOs with EMU Rompler characteristics
 */

#include "EMUModulation.h"
#include <algorithm>
#include <cmath>

//=============================================================================
// EMUEnvelope Implementation

EMUEnvelope::EMUEnvelope()
{
    reset();
}

void EMUEnvelope::prepareToPlay(double sampleRate_)
{
    sampleRate = sampleRate_;
}

float EMUEnvelope::getNextSample()
{
    applyPaintModulation();
    updateState();
    return currentLevel;
}

void EMUEnvelope::processBlock(float* buffer, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        buffer[i] = getNextSample();
    }
}

void EMUEnvelope::reset()
{
    currentState = Idle;
    currentLevel = 0.0f;
    targetLevel = 0.0f;
    stateTime = 0.0f;
}

void EMUEnvelope::noteOn()
{
    currentState = Attack;
    targetLevel = 1.0f;
    stateTime = 0.0f;
}

void EMUEnvelope::noteOff()
{
    if (currentState != Idle)
    {
        currentState = Release;
        targetLevel = 0.0f;
        stateTime = 0.0f;
    }
}

void EMUEnvelope::kill()
{
    currentState = Idle;
    currentLevel = 0.0f;
    targetLevel = 0.0f;
    stateTime = 0.0f;
}

void EMUEnvelope::setAttack(float timeInSeconds)
{
    attackTime = juce::jlimit(0.001f, 10.0f, timeInSeconds);
}

void EMUEnvelope::setDecay(float timeInSeconds)
{
    decayTime = juce::jlimit(0.001f, 10.0f, timeInSeconds);
}

void EMUEnvelope::setSustain(float level)
{
    sustainLevel = juce::jlimit(0.0f, 1.0f, level);
}

void EMUEnvelope::setRelease(float timeInSeconds)
{
    releaseTime = juce::jlimit(0.001f, 10.0f, timeInSeconds);
}

void EMUEnvelope::setAttackCurve(float curve)
{
    attackCurve = juce::jlimit(-1.0f, 1.0f, curve);
}

void EMUEnvelope::setDecayReleaseCurve(float curve)
{
    decayReleaseCurve = juce::jlimit(-1.0f, 1.0f, curve);
}

void EMUEnvelope::setVelocityAmount(float amount)
{
    velocityAmount = juce::jlimit(0.0f, 1.0f, amount);
}

void EMUEnvelope::setKeyTrackAmount(float amount)
{
    keyTrackAmount = juce::jlimit(0.0f, 1.0f, amount);
}

void EMUEnvelope::setPaintModulation(float pressure, float y)
{
    paintPressureMod = juce::jlimit(0.0f, 1.0f, pressure);
    paintYMod = juce::jlimit(0.0f, 1.0f, y);
}

void EMUEnvelope::setPaintMapping(int attackMap, int releaseMap)
{
    attackPaintMapping = juce::jlimit(0, 2, attackMap);
    releasePaintMapping = juce::jlimit(0, 2, releaseMap);
}

bool EMUEnvelope::isActive() const
{
    return currentState != Idle;
}

EMUEnvelope::EnvelopeData EMUEnvelope::getEnvelopeData() const
{
    EnvelopeData data;
    data.attackTime = attackTime;
    data.decayTime = decayTime;
    data.sustainLevel = sustainLevel;
    data.releaseTime = releaseTime;
    data.currentLevel = currentLevel;
    data.state = currentState;
    data.timeInState = stateTime;
    return data;
}

void EMUEnvelope::updateState()
{
    const float sampleTime = 1.0f / (float)sampleRate;
    stateTime += sampleTime;
    
    switch (currentState)
    {
        case Idle:
            currentLevel = 0.0f;
            break;
            
        case Attack:
            {
                float effectiveAttackTime = attackTime;
                
                // Apply paint modulation to attack time
                if (attackPaintMapping == 1) // Pressure
                {
                    effectiveAttackTime *= (0.1f + paintPressureMod * 0.9f); // 10%-100% of original time
                }
                else if (attackPaintMapping == 2) // Y-axis
                {
                    effectiveAttackTime *= (0.1f + paintYMod * 0.9f);
                }
                
                float attackRate = calculateRate(effectiveAttackTime, attackCurve);
                currentLevel += attackRate * sampleTime;
                
                if (currentLevel >= 1.0f || stateTime >= effectiveAttackTime)
                {
                    currentLevel = 1.0f;
                    currentState = Decay;
                    targetLevel = sustainLevel;
                    stateTime = 0.0f;
                }
            }
            break;
            
        case Decay:
            {
                float decayRate = calculateRate(decayTime, decayReleaseCurve);
                float decayAmount = (1.0f - sustainLevel) * decayRate * sampleTime;
                currentLevel -= decayAmount;
                
                if (currentLevel <= sustainLevel || stateTime >= decayTime)
                {
                    currentLevel = sustainLevel;
                    currentState = Sustain;
                    stateTime = 0.0f;
                }
            }
            break;
            
        case Sustain:
            currentLevel = sustainLevel;
            // Stay in sustain until noteOff
            break;
            
        case Release:
            {
                float effectiveReleaseTime = releaseTime;
                
                // Apply paint modulation to release time
                if (releasePaintMapping == 1) // Pressure
                {
                    effectiveReleaseTime *= (0.1f + paintPressureMod * 0.9f);
                }
                else if (releasePaintMapping == 2) // Y-axis
                {
                    effectiveReleaseTime *= (0.1f + paintYMod * 0.9f);
                }
                
                float releaseRate = calculateRate(effectiveReleaseTime, decayReleaseCurve);
                currentLevel -= currentLevel * releaseRate * sampleTime;
                
                if (currentLevel <= 0.001f || stateTime >= effectiveReleaseTime)
                {
                    currentLevel = 0.0f;
                    currentState = Idle;
                    stateTime = 0.0f;
                }
            }
            break;
    }
    
    // Apply velocity scaling
    if (velocityAmount > 0.0f)
    {
        currentLevel *= (1.0f - velocityAmount + velocityAmount * currentVelocity);
    }
}

float EMUEnvelope::calculateRate(float timeInSeconds, float curve) const
{
    if (timeInSeconds <= 0.001f)
        return 1000.0f; // Very fast
        
    float baseRate = 1.0f / timeInSeconds;
    
    // Apply curve shaping (exponential vs linear vs logarithmic)
    if (curve > 0.0f)
    {
        // Exponential curve (fast start, slow end)
        baseRate *= (1.0f + curve * 2.0f);
    }
    else if (curve < 0.0f)
    {
        // Logarithmic curve (slow start, fast end) 
        baseRate *= (1.0f + curve * -0.5f);
    }
    
    return baseRate;
}

float EMUEnvelope::applyCurve(float linear, float curve) const
{
    if (curve == 0.0f)
        return linear; // Linear
        
    if (curve > 0.0f)
    {
        // Exponential curve
        return std::pow(linear, 1.0f + curve);
    }
    else
    {
        // Logarithmic curve
        return 1.0f - std::pow(1.0f - linear, 1.0f - curve);
    }
}

void EMUEnvelope::applyPaintModulation()
{
    // Real-time modulation is already applied in updateState()
    // This could be used for additional real-time effects
}

//=============================================================================
// EMULFO Implementation

// Static wavetable initialization
std::array<float, EMULFO::WAVETABLE_SIZE> EMULFO::sineTable;
std::array<float, EMULFO::WAVETABLE_SIZE> EMULFO::triangleTable;
std::array<float, EMULFO::WAVETABLE_SIZE> EMULFO::sawTable;
bool EMULFO::tablesInitialized = false;

EMULFO::EMULFO()
{
    if (!tablesInitialized)
    {
        initializeWavetables();
        tablesInitialized = true;
    }
    
    reset();
}

void EMULFO::initializeWavetables()
{
    const float twoPi = 2.0f * juce::MathConstants<float>::pi;
    
    for (int i = 0; i < WAVETABLE_SIZE; ++i)
    {
        float phase = (float)i / WAVETABLE_SIZE;
        
        // Sine wave
        sineTable[i] = std::sin(twoPi * phase);
        
        // Triangle wave
        if (phase < 0.25f)
            triangleTable[i] = 4.0f * phase;
        else if (phase < 0.75f)
            triangleTable[i] = 2.0f - 4.0f * phase;
        else
            triangleTable[i] = 4.0f * phase - 4.0f;
            
        // Sawtooth wave
        sawTable[i] = 2.0f * phase - 1.0f;
    }
}

void EMULFO::prepareToPlay(double sampleRate_)
{
    sampleRate = sampleRate_;
    updatePhaseIncrement();
}

float EMULFO::getNextSample()
{
    applyPaintModulation();
    updateVintageCharacter();
    
    // Generate current value
    currentValue = generateWaveform(phase);
    
    // Apply depth scaling
    currentValue *= depth;
    
    // Apply fade-in if active
    if (fadeInTime > 0.0f && fadeInCounter < fadeInTime)
    {
        fadeInCounter += 1.0f / (float)sampleRate;
        fadeInGain = juce::jmin(1.0f, fadeInCounter / fadeInTime);
        currentValue *= fadeInGain;
    }
    
    // Apply vintage character
    if (vintageMode)
    {
        currentValue += analogDrift * 0.01f; // Small amount of drift
    }
    
    // Advance phase
    phase += phaseIncrement;
    if (phase >= 1.0f)
        phase -= 1.0f;
        
    return currentValue;
}

void EMULFO::processBlock(float* buffer, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        buffer[i] = getNextSample();
    }
}

void EMULFO::reset()
{
    phase = phaseOffset;
    currentValue = 0.0f;
    fadeInCounter = 0.0f;
    fadeInGain = fadeInTime > 0.0f ? 0.0f : 1.0f;
    updatePhaseIncrement();
}

void EMULFO::sync()
{
    phase = phaseOffset;
    fadeInCounter = 0.0f;
    fadeInGain = fadeInTime > 0.0f ? 0.0f : 1.0f;
}

void EMULFO::setRate(float hz)
{
    rate = juce::jlimit(0.01f, 100.0f, hz);
    updatePhaseIncrement();
}

void EMULFO::setDepth(float depth_)
{
    depth = juce::jlimit(0.0f, 1.0f, depth_);
}

void EMULFO::setWaveform(Waveform wave)
{
    waveform = wave;
}

void EMULFO::setPhaseOffset(float phase_)
{
    phaseOffset = phase_;
    if (phase == 0.0f) // Only apply if not running
        phase = phaseOffset;
}

void EMULFO::setSymmetry(float symmetry_)
{
    symmetry = juce::jlimit(0.0f, 1.0f, symmetry_);
}

void EMULFO::setBPMSync(bool enabled)
{
    bpmSyncEnabled = enabled;
    updatePhaseIncrement();
}

void EMULFO::setBPMRate(float bpm, int division)
{
    currentBPM = juce::jlimit(60.0f, 200.0f, bpm);
    syncDivision = juce::jlimit(1, 32, division);
    
    if (bpmSyncEnabled)
    {
        updatePhaseIncrement();
    }
}

void EMULFO::setFadeIn(float fadeTime)
{
    fadeInTime = juce::jmax(0.0f, fadeTime);
    
    if (fadeInTime > 0.0f && fadeInCounter == 0.0f)
    {
        fadeInGain = 0.0f;
    }
}

void EMULFO::setPaintModulation(float x, float y, float pressure)
{
    paintRateMod = juce::jlimit(0.0f, 1.0f, x);
    paintDepthMod = juce::jlimit(0.0f, 1.0f, pressure);
    paintWaveMod = juce::jlimit(0.0f, 1.0f, y);
}

void EMULFO::setPaintMapping(int rateMap, int depthMap, int waveMap)
{
    ratePaintMapping = juce::jlimit(0, 2, rateMap);
    depthPaintMapping = juce::jlimit(0, 2, depthMap);
    wavePaintMapping = juce::jlimit(0, 2, waveMap);
}

void EMULFO::setVintageMode(bool enabled)
{
    vintageMode = enabled;
}

void EMULFO::setTempoSync(bool enabled)
{
    tempoSyncEnabled = enabled;
    updatePhaseIncrement();
}

void EMULFO::updatePhaseIncrement()
{
    float effectiveRate = rate;
    
    if (bpmSyncEnabled || tempoSyncEnabled)
    {
        // Convert BPM and division to Hz
        float beatsPerSecond = currentBPM / 60.0f;
        float cyclesPerBeat = 1.0f / syncDivision;
        effectiveRate = beatsPerSecond * cyclesPerBeat;
    }
    
    phaseIncrement = effectiveRate / (float)sampleRate;
}

float EMULFO::generateWaveform(float phase) const
{
    float adjustedPhase = phase;
    
    // Apply phase offset
    adjustedPhase += phaseOffset;
    if (adjustedPhase >= 1.0f)
        adjustedPhase -= 1.0f;
        
    switch (waveform)
    {
        case Sine:
            {
                int index = (int)(adjustedPhase * WAVETABLE_SIZE) % WAVETABLE_SIZE;
                return sineTable[index];
            }
            
        case Triangle:
            {
                int index = (int)(adjustedPhase * WAVETABLE_SIZE) % WAVETABLE_SIZE;
                return triangleTable[index];
            }
            
        case Square:
            return (adjustedPhase < symmetry) ? 1.0f : -1.0f;
            
        case Sawtooth:
            {
                int index = (int)(adjustedPhase * WAVETABLE_SIZE) % WAVETABLE_SIZE;
                return sawTable[index];
            }
            
        case ReverseSaw:
            {
                int index = (int)(adjustedPhase * WAVETABLE_SIZE) % WAVETABLE_SIZE;
                return -sawTable[index];
            }
            
        case SampleAndHold:
            return const_cast<EMULFO*>(this)->applySampleAndHold();
            
        case Noise:
            return const_cast<juce::Random&>(vintageRandom).nextFloat() * 2.0f - 1.0f;
            
        default:
            return 0.0f;
    }
}

float EMULFO::applySampleAndHold()
{
    static float heldValue = 0.0f;
    static float lastPhase = 0.0f;
    
    // Generate new random value at phase reset
    if (phase < lastPhase)
    {
        heldValue = vintageRandom.nextFloat() * 2.0f - 1.0f;
    }
    
    lastPhase = phase;
    return heldValue;
}

void EMULFO::applyPaintModulation()
{
    // Apply paint modulation to parameters
    float effectiveRate = rate;
    float effectiveDepth = depth;
    
    if (ratePaintMapping == 1) // X-axis controls rate
    {
        effectiveRate *= (0.1f + paintRateMod * 2.0f); // 10% to 200% of base rate
        setRate(effectiveRate);
    }
    
    if (depthPaintMapping == 1) // Pressure controls depth
    {
        effectiveDepth = paintDepthMod;
        setDepth(effectiveDepth);
    }
    
    if (wavePaintMapping == 1) // Y-axis controls waveform
    {
        int waveIndex = (int)(paintWaveMod * 6.99f); // 0-6 waveforms
        setWaveform(static_cast<Waveform>(waveIndex));
    }
}

void EMULFO::updateVintageCharacter()
{
    if (vintageMode)
    {
        // Add subtle random drift to analog character
        analogDrift += (vintageRandom.nextFloat() - 0.5f) * 0.0001f;
        analogDrift *= 0.999f; // Slowly decay drift
        
        // Limit drift amount
        analogDrift = juce::jlimit(-0.01f, 0.01f, analogDrift);
    }
}

//=============================================================================
// EMUModMatrix Implementation

EMUModMatrix::EMUModMatrix()
{
    clearAllConnections();
    reset();
}

void EMUModMatrix::setConnection(int slot, ModSource source, ModDestination dest, float amount)
{
    if (slot >= 0 && slot < MAX_CONNECTIONS)
    {
        connections[slot].source = source;
        connections[slot].destination = dest;
        connections[slot].amount = juce::jlimit(-1.0f, 1.0f, amount);
        connections[slot].active = (amount != 0.0f);
    }
}

void EMUModMatrix::clearConnection(int slot)
{
    if (slot >= 0 && slot < MAX_CONNECTIONS)
    {
        connections[slot].source = None;
        connections[slot].amount = 0.0f;
        connections[slot].active = false;
    }
}

void EMUModMatrix::clearAllConnections()
{
    for (auto& connection : connections)
    {
        connection.source = None;
        connection.amount = 0.0f;
        connection.active = false;
    }
}

void EMUModMatrix::updateSources(const std::array<float, 16>& sourceValues)
{
    currentSources = sourceValues;
    
    // Clear destination values
    std::fill(destinationValues.begin(), destinationValues.end(), 0.0f);
    
    // Apply all active connections
    for (const auto& connection : connections)
    {
        if (connection.active && connection.source != None)
        {
            int srcIndex = static_cast<int>(connection.source);
            int destIndex = static_cast<int>(connection.destination);
            
            if (srcIndex < currentSources.size() && destIndex < destinationValues.size())
            {
                float modValue = currentSources[srcIndex] * connection.amount;
                destinationValues[destIndex] += modValue;
            }
        }
    }
}

float EMUModMatrix::getModulationFor(ModDestination destination) const
{
    int destIndex = static_cast<int>(destination);
    if (destIndex < destinationValues.size())
    {
        return juce::jlimit(-1.0f, 1.0f, destinationValues[destIndex]);
    }
    return 0.0f;
}

void EMUModMatrix::reset()
{
    std::fill(currentSources.begin(), currentSources.end(), 0.0f);
    std::fill(destinationValues.begin(), destinationValues.end(), 0.0f);
}

void EMUModMatrix::updatePaintSources(float x, float y, float pressure, juce::Colour color)
{
    paintX = juce::jlimit(0.0f, 1.0f, x);
    paintY = juce::jlimit(0.0f, 1.0f, y);
    paintPressure = juce::jlimit(0.0f, 1.0f, pressure);
    
    paintHue = color.getHue();
    paintSaturation = color.getSaturation();
    paintBrightness = color.getBrightness();
    
    // Update source array with paint values
    if (currentSources.size() >= 12) // Ensure we have room for paint sources
    {
        currentSources[8] = paintX;           // PaintX
        currentSources[9] = paintY;           // PaintY  
        currentSources[10] = paintPressure;   // PaintPressure
        currentSources[11] = paintBrightness; // PaintColor (using brightness)
    }
}

void EMUModMatrix::loadPresetMatrix(int presetNumber)
{
    clearAllConnections();
    
    // EMU-style preset modulation matrices
    switch (presetNumber)
    {
        case 0: // Classic Filter Sweep
            setConnection(0, LFO1, FilterCutoff, 0.7f);
            setConnection(1, Envelope1, FilterCutoff, 0.8f);
            setConnection(2, Velocity, FilterCutoff, 0.3f);
            break;
            
        case 1: // Paint-Controlled
            setConnection(0, PaintX, SamplePitch, 0.5f);
            setConnection(1, PaintY, FilterCutoff, 0.8f);
            setConnection(2, PaintPressure, FilterResonance, 0.6f);
            break;
            
        case 2: // Expression Setup
            setConnection(0, ModWheel, LFO1Depth, 1.0f);
            setConnection(1, LFO1, SamplePitch, 0.1f);
            setConnection(2, Velocity, EnvAttack, -0.4f);
            setConnection(3, KeyTrack, FilterCutoff, 0.5f);
            break;
            
        case 3: // Rhythmic
            setConnection(0, LFO2, SampleVolume, 0.6f);
            setConnection(1, LFO1, FilterCutoff, 0.4f);
            setConnection(2, Random, SampleStart, 0.3f);
            break;
            
        default:
            // No connections for unknown presets
            break;
    }
}

void EMUModMatrix::saveCurrentMatrix(int slotNumber)
{
    // This would save to persistent storage in a real implementation
    // For now, just a placeholder
    (void)slotNumber;
}