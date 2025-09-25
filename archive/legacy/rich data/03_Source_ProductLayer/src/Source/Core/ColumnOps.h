#pragma once
#include "AtlasIds.h"
#include <cstring>
#include <type_traits>

namespace atlas
{
    // Canonical sizes for a single spectral column
    constexpr size_t kColumnElems = static_cast<size_t>(AtlasConfig::NUM_BINS);
    constexpr size_t kColumnBytes = kColumnElems * sizeof(float);

    // Copy one column (NUM_BINS floats) with compile-time size enforcement
    template <typename DstArray, typename SrcArray>
    inline void copyColumn(DstArray& dst, const SrcArray& src) noexcept
    {
        static_assert(sizeof(dst) == kColumnBytes, "dst column size != NUM_BINS floats");
        static_assert(sizeof(src) == kColumnBytes, "src column size != NUM_BINS floats");
        std::memcpy(static_cast<void*>(&dst), static_cast<const void*>(&src), kColumnBytes);
    }
}