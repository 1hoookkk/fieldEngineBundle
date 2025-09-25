#pragma once
#include <atomic>
#include <memory>
#include "AtlasIds.h"
#include "ColumnOps.h"

class MaskAtlas
{
public:
    static constexpr int kW = AtlasConfig::TILE_WIDTH;
    static constexpr int kB = AtlasConfig::NUM_BINS;

    void prepare()
    {
        for (auto& p : pages_)
        {
            p.reset(new float[(size_t)kW * kB]);
            for (int c = 0; c < kW; ++c)
                for (int b = 0; b < kB; ++b)
                    p[(size_t)c * kB + b] = 1.0f;
        }
        active_.store(0, std::memory_order_release);
        gen_.store(0, std::memory_order_release);
    }

    inline void writeStagingColumn(int colInTile, const float* values) noexcept
    {
        if ((unsigned)colInTile >= (unsigned)kW) return;
        float* dst = &pages_[stagingIndex()][(size_t)colInTile * kB];
        struct Col { float v[kB]; };
        atlas::copyColumn(reinterpret_cast<Col&>(*dst).v,
                          *reinterpret_cast<const Col*>(values));
    }

    inline void flip() noexcept
    {
        const int a = active_.load(std::memory_order_relaxed);
        active_.store(1 - a, std::memory_order_release);
        gen_.fetch_add(1, std::memory_order_acq_rel);
    }

    inline const float* activeColumn(int colInTile) const noexcept
    {
        if ((unsigned)colInTile >= (unsigned)kW) return nullptr;
        return &pages_[active_.load(std::memory_order_acquire)][(size_t)colInTile * kB];
    }

    inline uint32_t generation() const noexcept { return gen_.load(std::memory_order_acquire); }

private:
    inline int stagingIndex() const noexcept { return 1 - active_.load(std::memory_order_relaxed); }

    std::unique_ptr<float[]> pages_[2]; // [page][col*kB + bin]
    std::atomic<int> active_{0};
    std::atomic<uint32_t> gen_{0};
};
static_assert(AtlasConfig::TILE_HEIGHT == AtlasConfig::NUM_BINS, "mask height mismatch");