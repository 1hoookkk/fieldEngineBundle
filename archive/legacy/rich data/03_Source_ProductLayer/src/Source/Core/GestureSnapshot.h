#pragma once

#define _USE_MATH_DEFINES
#include <algorithm>
#include <cmath>
#include <atomic>

/**
 * @file GestureSnapshot.h
 * @brief RT-safe POD gesture data structure with parameter mappings
 * 
 * SpectralCanvas Pro - Paint→Audio Pipeline
 * This header defines the core gesture capture format used in the lock-free
 * paint queue between UI thread and audio thread processing.
 */

namespace SpectralCanvas {

/**
 * @brief POD gesture snapshot structure
 * 
 * Captures all gesture parameters in normalized 0..1 (or -1..1) ranges
 * for RT-safe transmission through the lock-free paint queue.
 * All members are POD types suitable for atomic operations.
 */
struct GestureSnapshot {
    double pressure;   // 0..1 -> snap sigma mapping
    double hue;        // 0..1 -> LP/BP blend ratio  
    double size;       // 0..1 -> stereo spread width
    double speed;      // 0..1 -> attack smoothing rate
    double direction;  // -1..1 -> phase/lead/lag alignment
    
    // Default constructor for zero-initialization
    GestureSnapshot() : pressure(0.0), hue(0.0), size(0.0), speed(0.0), direction(0.0) {}
    
    // Explicit constructor for full initialization
    GestureSnapshot(double p, double h, double s, double sp, double d) 
        : pressure(p), hue(h), size(s), speed(sp), direction(d) {}
};

/**
 * @brief RT-safe parameter mapping utilities
 * 
 * All functions are inline and use only basic math operations
 * suitable for real-time audio thread execution.
 */
namespace GestureMapping {
    
    // Utility functions
    inline double clamp(double value, double min, double max) {
        return std::min(max, std::max(min, value));
    }
    
    inline double lerp(double a, double b, double t) {
        return a + t * (b - a);
    }
    
    /**
     * @brief Maps pressure to sigma cents for spectral snap precision
     * @param pressure Normalized pressure value (0..1)
     * @return Sigma value in cents (200.0 at pressure=0, 8.0 at pressure=1)
     */
    inline double pressureToSigmaCents(double pressure) {
        const double clampedPressure = clamp(pressure, 0.0, 1.0);
        return lerp(200.0, 8.0, clampedPressure);
    }
    
    /**
     * @brief Maps hue to LP/BP filter blend amounts
     * @param hue Normalized hue value (0..1)
     * @param[out] lpAmount Low-pass filter amount (1.0 at hue=0, 0.0 at hue=1)
     * @param[out] bpAmount Band-pass filter amount (0.0 at hue=0, 1.0 at hue=1)
     */
    inline void hueToFilterBlend(double hue, double& lpAmount, double& bpAmount) {
        const double clampedHue = clamp(hue, 0.0, 1.0);
        lpAmount = 1.0 - clampedHue;
        bpAmount = clampedHue;
    }
    
    /**
     * @brief Maps size to stereo spread phase offset
     * @param size Normalized size value (0..1)
     * @return Phase offset in radians (0.0 at size=0, ~1.047 at size=1)
     */
    inline double sizeToPhaseOffset(double size) {
        const double clampedSize = clamp(size, 0.0, 1.0);
        const double spreadDegrees = lerp(0.0, 60.0, clampedSize);
        return spreadDegrees * (3.14159265358979323846 / 180.0);  // Convert to radians
    }
    
    /**
     * @brief Maps speed to harmonic attack time
     * @param speed Normalized speed value (0..1)
     * @return Attack time in milliseconds (300.0 at speed=0, 1.0 at speed=1)
     */
    inline double speedToAttackMs(double speed) {
        const double clampedSpeed = clamp(speed, 0.0, 1.0);
        return lerp(1.0, 300.0, 1.0 - clampedSpeed);  // Inverted mapping
    }
    
    /**
     * @brief Maps direction to phase alignment sign
     * @param direction Direction value (-1..1)
     * @return Phase sign (+1 for direction >= 0, -1 for direction < 0)
     */
    inline double directionToPhaseSign(double direction) {
        return (direction >= 0.0) ? +1.0 : -1.0;
    }
    
    /**
     * @brief Complete gesture mapping for audio thread consumption
     * @param gesture Input gesture snapshot
     * @param[out] sigmaCents Snap precision in cents
     * @param[out] lpAmount Low-pass filter amount
     * @param[out] bpAmount Band-pass filter amount  
     * @param[out] phaseOffset Stereo phase offset in radians
     * @param[out] attackMs Attack time in milliseconds
     * @param[out] phaseSign Phase alignment sign
     */
    inline void mapGestureToAudioParams(const GestureSnapshot& gesture,
                                       double& sigmaCents,
                                       double& lpAmount,
                                       double& bpAmount,
                                       double& phaseOffset,
                                       double& attackMs,
                                       double& phaseSign) {
        sigmaCents = pressureToSigmaCents(gesture.pressure);
        hueToFilterBlend(gesture.hue, lpAmount, bpAmount);
        phaseOffset = sizeToPhaseOffset(gesture.size);
        attackMs = speedToAttackMs(gesture.speed);
        phaseSign = directionToPhaseSign(gesture.direction);
    }

} // namespace GestureMapping

/**
 * @brief RT-safe double-buffered gesture snapshot for audio thread access
 */
class GestureSnapshotBuffer {
public:
    static GestureSnapshotBuffer& instance() noexcept {
        static GestureSnapshotBuffer g;
        return g;
    }

    void pushSnapshot(const GestureSnapshot& s) noexcept {
        int next = (m_index.load(std::memory_order_relaxed) + 1) & 1;
        m_slots[next] = s;
        m_index.store(next, std::memory_order_release);
    }

    GestureSnapshot getCurrent() const noexcept {
        int idx = m_index.load(std::memory_order_acquire);
        return m_slots[idx];
    }

private:
    GestureSnapshotBuffer() noexcept {
        m_slots[0] = GestureSnapshot();
        m_slots[1] = GestureSnapshot();
        m_index.store(0, std::memory_order_relaxed);
    }

    GestureSnapshot m_slots[2];
    mutable std::atomic<int> m_index{0};
};

} // namespace SpectralCanvas