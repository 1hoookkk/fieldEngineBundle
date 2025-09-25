#pragma once
#include <cmath>
#include <algorithm>
#include "Snapper.h"

/**
 * Hard Tune Algorithm - Classic Auto-Tune Effect
 *
 * Key characteristics:
 * - Instant pitch quantization (0-5ms retune speed)
 * - No smoothing between notes - the artifacts ARE the effect
 * - Aggressive formant shifting for robotic character
 * - Works best with monophonic material
 */
class HardTune {
public:
    // Retune speed in milliseconds (0 = instant, 5 = Autotune 5 classic)
    void setRetuneSpeed(float ms) {
        retuneSpeed = std::clamp(ms, 0.0f, 100.0f);

        // Calculate smoothing factor (0 = no smooth, 1 = full smooth)
        // At 0ms, alpha = 1.0 (instant)
        // At 5ms, alpha = 0.95 (fast but not instant)
        // At 100ms, alpha = 0.5 (slow)
        if (retuneSpeed <= 0.0f) {
            smoothingAlpha = 1.0f; // Instant
        } else {
            // Exponential mapping for musical feel
            float normalized = retuneSpeed / 100.0f;
            smoothingAlpha = 1.0f - (normalized * 0.5f);
        }
    }

    // Formant shift amount (-12 to +12 semitones)
    void setFormantShift(float semitones) {
        formantShift = std::clamp(semitones, -12.0f, 12.0f);
    }

    // Throat length modeling (0.5 = child, 1.0 = normal, 1.5 = giant)
    void setThroatLength(float ratio) {
        throatRatio = std::clamp(ratio, 0.5f, 2.0f);
    }

    // Enable robot mode (extreme quantization + formant destruction)
    void setRobotMode(bool enable) {
        robotMode = enable;
    }

    // Process pitch correction
    // Returns the pitch ratio for the shifter
    float processPitch(float detectedMidi, float confidence) {
        // If no confident pitch detected, return unity
        if (confidence < 0.3f || detectedMidi <= 0.0f) {
            currentRatio = 1.0f;
            return currentRatio;
        }

        // Quantize to scale
        float targetMidi = snapper.snap(detectedMidi);

        // In robot mode, add octave jumps for dramatic effect
        if (robotMode) {
            // Occasionally jump octaves for T-Pain style flourishes
            if (detectedMidi > 72.0f) { // Above C5
                targetMidi += 12.0f; // Jump up an octave
            } else if (detectedMidi < 48.0f) { // Below C3
                targetMidi -= 12.0f; // Jump down an octave
            }
        }

        // Calculate pitch ratio (for pitch shifter)
        float targetFreq = 440.0f * std::pow(2.0f, (targetMidi - 69.0f) / 12.0f);
        float detectedFreq = 440.0f * std::pow(2.0f, (detectedMidi - 69.0f) / 12.0f);
        float targetRatio = targetFreq / detectedFreq;

        // Apply retune speed smoothing
        if (smoothingAlpha < 1.0f) {
            // Smooth the ratio change
            currentRatio = currentRatio + smoothingAlpha * (targetRatio - currentRatio);
        } else {
            // Instant snap (classic hard tune)
            currentRatio = targetRatio;
        }

        // Limit extreme ratios to prevent artifacts
        currentRatio = std::clamp(currentRatio, 0.25f, 4.0f);

        return currentRatio;
    }

    // Get formant shift ratio for formant processing
    float getFormantRatio() const {
        if (robotMode) {
            // In robot mode, inverse formants for metallic sound
            return 1.0f / throatRatio;
        }
        return throatRatio * std::pow(2.0f, formantShift / 12.0f);
    }

    // Configure scale/key
    void setScale(int rootKey, int scaleType) {
        snapper.setKey(rootKey, scaleType);
    }

    // Reset state (call on playback start)
    void reset() {
        currentRatio = 1.0f;
    }

    // Get current pitch ratio for display
    float getCurrentRatio() const { return currentRatio; }

    // Get correction amount in cents for display
    float getCorrectionCents() const {
        return 1200.0f * std::log2(currentRatio);
    }

private:
    Snapper snapper;

    float retuneSpeed = 0.0f;      // 0-100ms (0 = instant hard tune)
    float smoothingAlpha = 1.0f;    // Smoothing factor
    float currentRatio = 1.0f;      // Current pitch shift ratio

    float formantShift = 0.0f;      // Formant shift in semitones
    float throatRatio = 1.0f;       // Throat length ratio
    bool robotMode = false;         // Extreme robot voice mode
};