#pragma once
#include <cstdint>
#include <type_traits>

/**
 * POD struct for color→spectral parameter updates.
 * Used for RT-safe communication from UI to audio thread.
 */
struct ParameterUpdate {
    int id = 0;           // parameter identifier
    float value = 0.0f;   // parameter value
    uint32_t timestamp = 0; // optional timestamp
};

// Ensure RT safety
static_assert(std::is_trivially_copyable_v<ParameterUpdate>, "ParameterUpdate must be trivially copyable for RT safety");
static_assert(std::is_standard_layout_v<ParameterUpdate>, "ParameterUpdate must have standard layout");