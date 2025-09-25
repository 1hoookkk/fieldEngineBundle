#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

// Zero-latency pitch tracker using AMDF (Average Magnitude Difference Function)
// Optimized for monophonic vocals in range ~70-800 Hz
class PitchTracker {
public:
    void prepare(double sampleRate, double hopSamples) {
        fs = sampleRate;
        hopSize = int(hopSamples);

        // Vocal range: ~70-800 Hz
        minPeriod = int(fs / 800.0); // ~60 samples @ 48kHz
        maxPeriod = int(fs / 70.0);  // ~686 samples @ 48kHz

        // Buffer size needs to accommodate max period + analysis window
        bufferSize = maxPeriod * 2;
        buffer.resize(bufferSize, 0.0f);

        writeIndex = 0;
        samplesSinceLastAnalysis = 0;
        lastGoodF0 = 0.0f;
        confidence = 0.0f;

        // Clear difference buffer
        int maxLag = maxPeriod - minPeriod + 1;
        diffBuffer.resize(maxLag, 0.0f);
    }

    void pushSamples(const float* samples, int numSamples) {
        for (int i = 0; i < numSamples; ++i) {
            buffer[writeIndex] = samples[i];
            writeIndex = (writeIndex + 1) % bufferSize;

            if (++samplesSinceLastAnalysis >= hopSize) {
                analyzeCurrentWindow();
                samplesSinceLastAnalysis = 0;
            }
        }
    }

    float getHz() const {
        // Return last good F0 if confidence is sufficient
        return (confidence > 0.3f) ? lastGoodF0 : 0.0f;
    }

    float getConfidence() const { return confidence; }

private:
    double fs = 48000.0;
    int hopSize = 144;  // ~3ms @ 48kHz
    int bufferSize = 1400;
    int minPeriod = 60;
    int maxPeriod = 686;

    std::vector<float> buffer;
    std::vector<float> diffBuffer;
    int writeIndex = 0;
    int samplesSinceLastAnalysis = 0;

    float lastGoodF0 = 0.0f;
    float confidence = 0.0f;

    void analyzeCurrentWindow() {
        // Get current analysis window (going backwards from write index)
        const int windowSize = maxPeriod;

        // Compute AMDF for different lag values with robustness guards
        float minDiff = 1e10f;
        int bestLag = minPeriod;
        bool validAnalysis = false;

        for (int lag = minPeriod; lag <= maxPeriod; ++lag) {
            float diff = 0.0f;
            int samples = 0;

            // Bounds checking for safety
            const int maxSamples = std::min(windowSize - lag, bufferSize / 2);
            if (maxSamples <= 0) continue;

            for (int i = 0; i < maxSamples; ++i) {
                int idx1 = (writeIndex - 1 - i + bufferSize) % bufferSize;
                int idx2 = (writeIndex - 1 - i - lag + bufferSize) % bufferSize;

                // NaN/Inf protection
                float val1 = buffer[idx1];
                float val2 = buffer[idx2];
                if (!std::isfinite(val1) || !std::isfinite(val2)) continue;

                diff += std::abs(val1 - val2);
                samples++;
            }

            if (samples > minPeriod) {  // Require minimum samples for validity
                diff /= samples; // Average

                // NaN/Inf guard on result
                if (std::isfinite(diff) && diff >= 0.0f && diff < minDiff) {
                    minDiff = diff;
                    bestLag = lag;
                    validAnalysis = true;
                }
            }
        }

        if (!validAnalysis) {
            // Analysis failed - decay confidence and exit
            confidence *= 0.7f;
            return;
        }

        // Convert lag to frequency with safety checks
        if (bestLag <= 0 || !std::isfinite(fs)) {
            confidence *= 0.7f;
            return;
        }

        float f0 = float(fs / bestLag);

        // Octave error detection and correction
        f0 = correctOctaveErrors(f0);

        // Enhanced voicing detection
        float energy = computeEnergy();
        float amdfRatio = computeAmdfRatio(minDiff, bestLag);
        float harmonicStrength = validateHarmonics(f0);

        // Robust confidence calculation
        confidence = std::min({energy, amdfRatio, harmonicStrength});
        confidence = std::clamp(confidence, 0.0f, 1.0f);

        // Adaptive confidence threshold based on sample rate
        const float confThreshold = 0.25f + 0.05f * (float(fs) / 48000.0f - 1.0f);

        // Update F0 with hysteresis to prevent flicker
        if (confidence > confThreshold) {
            // Smooth transition to new F0 to prevent jumps
            if (lastGoodF0 > 0.0f) {
                const float maxJump = lastGoodF0 * 0.15f; // 15% max jump
                if (std::abs(f0 - lastGoodF0) > maxJump) {
                    f0 = lastGoodF0 + std::copysign(maxJump, f0 - lastGoodF0);
                }
            }
            lastGoodF0 = f0;
        }
        // Confidence decay with hysteresis
        else if (confidence < confThreshold * 0.5f) {
            confidence *= 0.85f; // Gradual fade
        }
    }

    float computeEnergy() {
        float energy = 0.0f;
        const int windowSize = std::min(hopSize * 4, bufferSize / 2);
        int validSamples = 0;

        for (int i = 0; i < windowSize; ++i) {
            int idx = (writeIndex - 1 - i + bufferSize) % bufferSize;
            float sample = buffer[idx];

            // NaN/Inf protection
            if (std::isfinite(sample)) {
                energy += sample * sample;
                validSamples++;
            }
        }

        if (validSamples == 0) return 0.0f;

        energy /= validSamples;

        // Sample-rate aware energy scaling (compensate for different hop rates)
        const float energyScale = 2000.0f * (48000.0f / float(fs));
        return std::clamp(energy * energyScale, 0.0f, 1.0f);
    }

    float computeAmdfRatio(float minDiff, int bestLag) {
        (void)bestLag;  // Mark as intentionally unused
        // Compare minimum difference to average difference with robustness
        float avgDiff = 0.0f;
        int count = 0;
        const float epsilon = 1e-8f;

        // Sample across the lag range for comparison
        for (int lag = minPeriod; lag <= maxPeriod; lag += 4) {
            float diff = 0.0f;
            int samples = 0;
            const int maxSamples = std::min(maxPeriod - lag, bufferSize / 4);
            if (maxSamples <= 0) continue;

            for (int i = 0; i < maxSamples; i += 2) { // Subsample for speed
                int idx1 = (writeIndex - 1 - i + bufferSize) % bufferSize;
                int idx2 = (writeIndex - 1 - i - lag + bufferSize) % bufferSize;

                float val1 = buffer[idx1];
                float val2 = buffer[idx2];

                // NaN/Inf protection
                if (std::isfinite(val1) && std::isfinite(val2)) {
                    diff += std::abs(val1 - val2);
                    samples++;
                }
            }

            if (samples > 0) {
                float lagDiff = diff / samples;
                if (std::isfinite(lagDiff) && lagDiff >= 0.0f) {
                    avgDiff += lagDiff;
                    count++;
                }
            }
        }

        if (count > 0 && avgDiff > epsilon) {
            avgDiff /= count;

            // Robust ratio calculation with guards
            if (std::isfinite(minDiff) && std::isfinite(avgDiff) && avgDiff > epsilon) {
                float ratio = 1.0f - (minDiff / avgDiff);
                return std::clamp(ratio, 0.0f, 1.0f);
            }
        }

        return 0.0f;
    }

    // Octave error correction using harmonic validation
    float correctOctaveErrors(float f0) {
        if (f0 <= 0.0f || !std::isfinite(f0)) return 0.0f;

        // Check against reasonable vocal range (80-800 Hz)
        if (f0 < 80.0f) {
            // Might be octave down - try doubling
            float f0x2 = f0 * 2.0f;
            if (f0x2 <= 800.0f && validateHarmonics(f0x2) > validateHarmonics(f0)) {
                return f0x2;
            }
        }
        else if (f0 > 600.0f) {
            // Might be octave up - try halving
            float f0half = f0 * 0.5f;
            if (f0half >= 80.0f && validateHarmonics(f0half) > validateHarmonics(f0)) {
                return f0half;
            }
        }

        return f0;
    }

    // Simple harmonic validation for octave error detection
    float validateHarmonics(float f0) {
        if (f0 <= 0.0f || !std::isfinite(f0)) return 0.0f;

        // Check if this F0 has reasonable harmonic content
        const int period = int(fs / f0);
        if (period < minPeriod || period > maxPeriod) return 0.0f;

        // Simple autocorrelation at the proposed period
        float correlation = 0.0f;
        int samples = 0;
        const int checkSamples = std::min(period * 2, bufferSize / 3);

        for (int i = 0; i < checkSamples; ++i) {
            int idx1 = (writeIndex - 1 - i + bufferSize) % bufferSize;
            int idx2 = (writeIndex - 1 - i - period + bufferSize) % bufferSize;

            float val1 = buffer[idx1];
            float val2 = buffer[idx2];

            if (std::isfinite(val1) && std::isfinite(val2)) {
                correlation += val1 * val2;
                samples++;
            }
        }

        return (samples > 0) ? std::clamp(correlation / samples, 0.0f, 1.0f) : 0.0f;
    }
};