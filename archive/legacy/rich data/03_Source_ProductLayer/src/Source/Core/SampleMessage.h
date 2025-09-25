#pragma once
#include <cstdint>
#include <type_traits>

// Trivial, handle-based message (no refcounts in RT).
struct SampleMessage {
    uint32_t handle = 0;   // 0 = invalid/no-op
    uint64_t tscOrUsec = 0; // optional timestamp for UI→Audio latency
    
    bool isValid() const noexcept { return handle != 0; }
};
static_assert(std::is_trivially_copyable<SampleMessage>::value, "RT message must be trivial");