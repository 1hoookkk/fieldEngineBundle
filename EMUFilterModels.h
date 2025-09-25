#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>

// EMU Filter Models enum
enum class EMUFilterModel
{
    HyperQ12 = 0,
    HyperQ6,
    PhaserForm,
    VocalMorph,
    NumModels
};

// Biquad filter for cascade implementation
class BiquadFilter
{
public:
    void reset()
    {
        state[0] = state[1] = 0.0f;
    }
    
    void setCoefficients(float b0_, float b1_, float b2_, float a1_, float a2_)
    {
        b0 = b0_;
        b1 = b1_;
        b2 = b2_;
        a1 = a1_;
        a2 = a2_;
    }
    
    float processSample(float input)
    {
        float output = b0 * input + state[0];
        state[0] = b1 * input - a1 * output + state[1];
        state[1] = b2 * input - a2 * output;
        return output;
    }
    
private:
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
    float a1 = 0.0f, a2 = 0.0f;
    float state[2] = {0.0f, 0.0f};
};

// Allpass filter for phaser implementation
class AllpassFilter
{
public:
    void reset()
    {
        state = 0.0f;
    }
    
    void setCoefficient(float coeff)
    {
        a = coeff;
    }
    
    float processSample(float input)
    {
        float output = -input + a * (input - state);
        state = input + a * output;
        return output;
    }
    
private:
    float a = 0.0f;
    float state = 0.0f;
};

// Main EMU Filter Model processor
class EMUFilterModelProcessor
{
public:
    EMUFilterModelProcessor() = default;
    ~EMUFilterModelProcessor() = default;
    
    void prepareToPlay(double sampleRate_)
    {
        sampleRate = sampleRate_;
        reset();
    }
    
    void reset()
    {
        // Reset all filters
        for (auto& biquad : hyperQStages)
            biquad.reset();
        for (auto& allpass : phaserStages)
            allpass.reset();
    }
    
    void setModel(EMUFilterModel model)
    {
        currentModel = model;
        updateCoefficients();
    }
    
    void setCutoffFrequency(float frequency)
    {
        cutoffFreq = frequency;
        updateCoefficients();
    }
    
    void setResonance(float resonance_)
    {
        resonance = juce::jlimit(0.0f, 1.0f, resonance_);
        updateCoefficients();
    }
    
    void setMorphPosition(float position)
    {
        morphPosition = juce::jlimit(0.0f, 1.0f, position);
        updateCoefficients();
    }
    
    float processSample(float input)
    {
        switch (currentModel)
        {
            case EMUFilterModel::HyperQ12:
                return processHyperQ12(input);
                
            case EMUFilterModel::HyperQ6:
                return processHyperQ6(input);
                
            case EMUFilterModel::PhaserForm:
                return processPhaserForm(input);
                
            case EMUFilterModel::VocalMorph:
                return processVocalMorph(input);
                
            default:
                return input;
        }
    }
    
    static juce::String getModelName(EMUFilterModel model)
    {
        switch (model)
        {
            case EMUFilterModel::HyperQ12: return "HyperQ 12";
            case EMUFilterModel::HyperQ6:  return "HyperQ 6";
            case EMUFilterModel::PhaserForm: return "PhaserForm";
            case EMUFilterModel::VocalMorph: return "VocalMorph";
            default: return "Unknown";
        }
    }
    
private:
    EMUFilterModel currentModel = EMUFilterModel::HyperQ12;
    double sampleRate = 44100.0;
    float cutoffFreq = 1000.0f;
    float resonance = 0.0f;
    float morphPosition = 0.0f;
    
    // Filter stages
    std::array<BiquadFilter, 12> hyperQStages;
    std::array<AllpassFilter, 8> phaserStages;
    
    void updateCoefficients()
    {
        switch (currentModel)
        {
            case EMUFilterModel::HyperQ12:
            case EMUFilterModel::HyperQ6:
                updateHyperQCoefficients();
                break;
                
            case EMUFilterModel::PhaserForm:
                updatePhaserCoefficients();
                break;
                
            case EMUFilterModel::VocalMorph:
                updateVocalMorphCoefficients();
                break;
        }
    }
    
    void updateHyperQCoefficients()
    {
        // Calculate normalized frequency
        float omega = 2.0f * juce::MathConstants<float>::pi * cutoffFreq / static_cast<float>(sampleRate);
        float sinOmega = std::sin(omega);
        float cosOmega = std::cos(omega);
        
        // Q factor increases with resonance
        float q = 0.707f + resonance * 10.0f;
        float alpha = sinOmega / (2.0f * q);
        
        // Lowpass biquad coefficients
        float b0 = (1.0f - cosOmega) / 2.0f;
        float b1 = 1.0f - cosOmega;
        float b2 = (1.0f - cosOmega) / 2.0f;
        float a0 = 1.0f + alpha;
        float a1 = -2.0f * cosOmega;
        float a2 = 1.0f - alpha;
        
        // Normalize coefficients
        b0 /= a0;
        b1 /= a0;
        b2 /= a0;
        a1 /= a0;
        a2 /= a0;
        
        // Set coefficients for all stages with slight variations
        int numStages = (currentModel == EMUFilterModel::HyperQ12) ? 12 : 6;
        for (int i = 0; i < numStages; ++i)
        {
            // Slight frequency spread for analog character
            float freqSpread = 1.0f + (i - numStages/2) * 0.01f;
            hyperQStages[i].setCoefficients(b0 * freqSpread, b1, b2 * freqSpread, a1, a2);
        }
    }
    
    void updatePhaserCoefficients()
    {
        // Phaser uses allpass filters with coefficient based on cutoff
        float baseCoeff = (cutoffFreq - 440.0f) / (cutoffFreq + 440.0f);
        
        // Distribute stages across frequency range
        for (int i = 0; i < 8; ++i)
        {
            float stageOffset = static_cast<float>(i) / 8.0f;
            float coeff = baseCoeff + stageOffset * 0.2f * (1.0f - baseCoeff);
            phaserStages[i].setCoefficient(coeff);
        }
    }
    
    void updateVocalMorphCoefficients()
    {
        // Vocal morph uses formant-style filtering
        // This would typically load formant data from the EMU banks
        // For now, using placeholder implementation
        updateHyperQCoefficients(); // Placeholder
    }
    
    float processHyperQ12(float input)
    {
        float output = input;
        
        // Process through 12 cascaded stages
        for (int i = 0; i < 12; ++i)
        {
            output = hyperQStages[i].processSample(output);
            
            // Add resonance feedback at specific stages
            if (i == 5 || i == 11)
            {
                output += resonance * 0.5f * output;
            }
        }
        
        // Soft saturation
        return std::tanh(output);
    }
    
    float processHyperQ6(float input)
    {
        float output = input;
        
        // Process through 6 cascaded stages
        for (int i = 0; i < 6; ++i)
        {
            output = hyperQStages[i].processSample(output);
        }
        
        // Add resonance feedback
        output += resonance * output;
        
        return std::tanh(output);
    }
    
    float processPhaserForm(float input)
    {
        float output = input;
        
        // Process through 8 allpass stages
        for (auto& stage : phaserStages)
        {
            output = stage.processSample(output);
        }
        
        // Mix with dry signal for phaser effect
        return input * (1.0f - morphPosition) + output * morphPosition;
    }
    
    float processVocalMorph(float input)
    {
        // Placeholder - would implement formant morphing
        return processHyperQ6(input);
    }
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EMUFilterModelProcessor)
};
