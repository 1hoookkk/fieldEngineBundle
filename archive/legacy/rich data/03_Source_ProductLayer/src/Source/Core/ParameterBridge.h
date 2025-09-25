// Core/ParameterBridge.h
#pragma once
#include <JuceHeader.h>
#include <atomic>

class ParameterBridge
{
public:
    struct SlotParameters
    {
        std::atomic<float> pitch{0.0f};
        std::atomic<float> speed{1.0f};
        std::atomic<float> volume{0.7f};
        std::atomic<float> drive{1.0f};
        std::atomic<float> crush{16.0f};
        std::atomic<bool> syncEnabled{false};
        std::atomic<bool> isPlaying{false};
        std::atomic<float> playProgress{0.0f};
    };

    ParameterBridge() = default;
    
    SlotParameters& getSlotParams(int slot) 
    { 
        jassert(slot >= 0 && slot < 8);
        return slotParams[slot]; 
    }

private:
    std::array<SlotParameters, 8> slotParams;
};