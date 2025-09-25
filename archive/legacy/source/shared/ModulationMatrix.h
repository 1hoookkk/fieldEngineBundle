#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <array>

// EMU-style modulation matrix implementation
class ModulationMatrix
{
public:
    // Modulation sources (EMU Audity 2000 style)
    enum ModSource
    {
        LFO1 = 0,
        LFO2,
        ENV1,
        ENV2,
        ENV_AUX,
        VELOCITY,
        KEY_TRACK,
        MOD_WHEEL,
        AFTERTOUCH,
        PITCH_BEND,
        EXPRESSION,
        BREATH,
        NumSources
    };
    
    // Modulation destinations
    enum ModTarget
    {
        FILTER_CUTOFF = 0,
        FILTER_RESONANCE,
        FILTER_MORPH,
        OSCILLATOR_PITCH,
        OSCILLATOR_PULSE_WIDTH,
        AMPLITUDE,
        PAN,
        LFO1_RATE,
        LFO2_RATE,
        ENV_ATTACK,
        ENV_DECAY,
        ENV_SUSTAIN,
        ENV_RELEASE,
        NumTargets
    };
    
    // Single modulation connection (patch cord)
    struct ModConnection
    {
        ModSource source = LFO1;
        ModTarget target = FILTER_CUTOFF;
        float amount = 0.0f;
        bool bipolar = true;  // True for -1 to +1, false for 0 to 1
        bool active = false;
    };
    
    ModulationMatrix() = default;
    ~ModulationMatrix() = default;
    
    // Add or update a modulation connection
    void setConnection(int slot, ModSource source, ModTarget target, float amount, bool bipolar = true)
    {
        if (slot >= 0 && slot < kMaxConnections)
        {
            connections[slot] = { source, target, amount, bipolar, true };
        }
    }
    
    // Remove a modulation connection
    void clearConnection(int slot)
    {
        if (slot >= 0 && slot < kMaxConnections)
        {
            connections[slot].active = false;
        }
    }
    
    // Set source value (called by synth engine)
    void setSourceValue(ModSource source, float value)
    {
        if (source < NumSources)
        {
            sourceValues[source] = value;
        }
    }
    
    // Get modulated value for a destination
    float getModulatedValue(ModTarget target, float baseValue = 0.0f)
    {
        float modulation = 0.0f;
        
        for (const auto& conn : connections)
        {
            if (conn.active && conn.target == target)
            {
                float srcValue = sourceValues[conn.source];
                
                // Apply bipolar/unipolar conversion if needed
                if (!conn.bipolar && srcValue < 0.0f)
                {
                    srcValue = (srcValue + 1.0f) * 0.5f;  // Convert -1,1 to 0,1
                }
                
                modulation += srcValue * conn.amount;
            }
        }
        
        return baseValue + modulation;
    }
    
    // Get raw modulation amount for a destination (without base value)
    float getModulationAmount(ModTarget target)
    {
        return getModulatedValue(target, 0.0f);
    }
    
    // Reset all connections
    void reset()
    {
        for (auto& conn : connections)
        {
            conn.active = false;
        }
        sourceValues.fill(0.0f);
    }
    
    // Get connection info
    const ModConnection& getConnection(int slot) const
    {
        static ModConnection empty;
        return (slot >= 0 && slot < kMaxConnections) ? connections[slot] : empty;
    }
    
    // EMU Audity 2000 has 24 patch cords per instrument
    static constexpr int kMaxConnections = 24;
    
private:
    std::array<ModConnection, kMaxConnections> connections;
    std::array<float, NumSources> sourceValues{};
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModulationMatrix)
};
