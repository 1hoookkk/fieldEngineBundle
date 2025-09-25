#pragma once
#include <atomic>
#include <cstdint>
#include <array>
#include <optional>
#include <type_traits>

// Trivial view of sample data (UI-owned). Deinterleaved channel pointers.
struct SampleView {
    const float* const* channels = nullptr; // array of channel pointers (UI lifetime)
    uint32_t numChannels = 0;
    uint32_t numFrames  = 0;
    double   sampleRate = 0.0;
    
    SampleView() = default;
    SampleView(const float* const* ch, uint32_t numCh, uint32_t numFr, double sr)
        : channels(ch), numChannels(numCh), numFrames(numFr), sampleRate(sr) {}
};
static_assert(std::is_trivially_copyable<SampleView>::value, "SampleView must be trivial");

// Lock-free fixed-slot pool with generation-checked 32-bit handles.
// Handle layout: [ gen:24 | idx:8 ]
class SamplePool {
public:
    static constexpr uint32_t kSlots   = 8;
    static constexpr uint32_t kIdxMask = 0xFFu;
    static inline uint8_t  idxOf(uint32_t h) { return static_cast<uint8_t>(h & kIdxMask); }
    static inline uint32_t genOf(uint32_t h) { return h >> 8; }
    static inline uint32_t makeHandle(uint8_t idx, uint32_t gen) { return (gen << 8) | idx; }

    // UI thread: reserve an empty slot. Returns handle (in FILLING state) or nullopt.
    std::optional<uint32_t> allocate() noexcept {
        for (uint8_t i = 0; i < kSlots; ++i) {
            uint32_t expected = Slot::Empty;
            if (slots_[i].state.compare_exchange_strong(expected, Slot::Filling, std::memory_order_acq_rel)) {
                const uint32_t gen = slots_[i].generation.load(std::memory_order_relaxed);
                return makeHandle(i, gen);
            }
        }
        return std::nullopt;
    }

    // UI thread: fill view and publish as READY.
    bool publish(uint32_t handle, const SampleView& view) noexcept {
        const uint8_t i = idxOf(handle);
        if (i >= kSlots) return false;
        Slot& s = slots_[i];
        if (genOf(handle) != s.generation.load(std::memory_order_acquire)) return false;
        uint32_t st = s.state.load(std::memory_order_acquire);
        if (st != Slot::Filling) return false;

        // Write payload, then publish READY with release.
        s.view = view; // trivially copyable; UI owns pointed memory
        s.state.store(Slot::Ready, std::memory_order_release);
        return true;
    }

    // UI thread: free a handle (READY or FILLING). Increments generation to invalidate stale readers.
    bool free(uint32_t handle) noexcept {
        const uint8_t i = idxOf(handle);
        if (i >= kSlots) return false;
        Slot& s = slots_[i];
        if (genOf(handle) != s.generation.load(std::memory_order_acquire)) return false;
        uint32_t st = s.state.load(std::memory_order_acquire);
        if (st != Slot::Ready && st != Slot::Filling) return false;

        s.state.store(Slot::Empty, std::memory_order_release);
        s.generation.fetch_add(1, std::memory_order_acq_rel);
        return true;
    }

    // Audio thread: acquire a snapshot if the handle is valid & READY.
    std::optional<SampleView> lookup(uint32_t handle) const noexcept {
        const uint8_t i = idxOf(handle);
        if (i >= kSlots) return std::nullopt;
        const Slot& s = slots_[i];

        if (s.state.load(std::memory_order_acquire) != Slot::Ready) return std::nullopt;
        if (genOf(handle) != s.generation.load(std::memory_order_acquire)) return std::nullopt;

        // Read view after acquire — safe snapshot.
        return s.view;
    }

    struct Stats { uint32_t ready=0, filling=0, empty=0; };
    Stats stats() const noexcept {
        Stats st;
        for (uint8_t i=0;i<kSlots;++i) {
            switch (slots_[i].state.load(std::memory_order_relaxed)) {
                case Slot::Ready:   ++st.ready;   break;
                case Slot::Filling: ++st.filling; break;
                default:            ++st.empty;   break;
            }
        }
        return st;
    }

private:
    struct Slot {
        enum : uint32_t { Empty=0, Filling=1, Ready=2 };
        std::atomic<uint32_t> state{Empty};
        std::atomic<uint32_t> generation{1}; // start at 1 to avoid zero-handle edge cases
        SampleView            view{};        // published under release/acquire via state
    };
    std::array<Slot, kSlots> slots_{};
};

// Extend/compose with your existing RT arenas/buffers if needed.
class RealtimeMemoryManager {
public:
    SamplePool samples;
    // ... add your other RT resources here
};

// Global singleton access (matches existing usage pattern)
class RealtimeMemorySystem {
public:
    static RealtimeMemorySystem& getInstance() noexcept {
        static RealtimeMemorySystem instance;
        return instance;
    }
    
    RealtimeMemoryManager& getManager() noexcept { return manager_; }
    SamplePool& samples() noexcept { return manager_.samples; }

private:
    RealtimeMemoryManager manager_;
};