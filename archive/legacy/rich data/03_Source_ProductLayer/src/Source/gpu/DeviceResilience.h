#pragma once

#include <atomic>
#include <cstdint>
#include "../Core/GPUStatus.h"

// Minimal device-lost handling scaffolding for tests.

class DeviceRecoveryTimer {
public:
    DeviceRecoveryTimer() noexcept : start_(nowMicros()) {}

    uint32_t getElapsedMicroseconds() const noexcept {
        const uint64_t end = nowMicros();
        const uint64_t delta = (end > start_) ? (end - start_) : 0ull;
        return static_cast<uint32_t>(delta > 0xffffffffull ? 0xffffffffull : delta);
    }

private:
    static uint64_t nowMicros() noexcept {
        using clock = std::chrono::steady_clock;
        const auto t = clock::now().time_since_epoch();
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(t).count());
    }

    uint64_t start_;
};

class DeviceLostHandler {
public:
    void initialize(GPUStatus* status) noexcept { gpu_status_ = status; }

    bool shouldAttemptRecovery() const noexcept {
        return gpu_status_ && gpu_status_->getDeviceState() == GPUStatus::REMOVED;
    }

    bool shouldFallbackToWarp() const noexcept {
        return consecutive_failures_.load(std::memory_order_relaxed) >= 3;
    }

    void beginRecovery() noexcept {
        if (gpu_status_) gpu_status_->setDeviceState(GPUStatus::RECREATING);
        timer_ = DeviceRecoveryTimer();
    }

    void recordSuccessfulRecovery(bool usedWarp) noexcept {
        if (!gpu_status_) return;
        gpu_status_->incrementRecoveryCount();
        gpu_status_->recordRecoveryTimestamp(DeviceRecoveryTimer().getElapsedMicroseconds());
        if (usedWarp) gpu_status_->setDeviceState(GPUStatus::WARP_FALLBACK);
        else gpu_status_->setDeviceState(GPUStatus::OK);
        consecutive_failures_.store(0, std::memory_order_relaxed);
    }

    void recordFailedRecovery() noexcept {
        consecutive_failures_.fetch_add(1, std::memory_order_relaxed);
    }

private:
    GPUStatus* gpu_status_ = nullptr;
    std::atomic<uint32_t> consecutive_failures_{0};
    DeviceRecoveryTimer timer_{};
};


