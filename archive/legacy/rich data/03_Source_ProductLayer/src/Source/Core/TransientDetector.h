// TransientDetector.h
// RT-safe hybrid transient detection using spectral flux + amplitude threshold
// No allocations in audio thread, preallocated circular buffers
#pragma once

#include <array>
#include <cmath>
#include <algorithm>
#include <numeric>

namespace scp {

class TransientDetector {
public:
    // Configuration from plugin designer
    static constexpr int kFFTSize = 512;
    static constexpr int kHopSize = 256;
    static constexpr int kNumBins = kFFTSize / 2 + 1;
    static constexpr float kSpectralFluxMultiplier = 2.5f;
    static constexpr float kAmplitudeThreshold = 0.35f;
    static constexpr int kLookbackFrames = 4;
    static constexpr int kTransientHoldFrames = 3; // ~60ms at hop=256
    
    TransientDetector() noexcept {
        reset();
    }
    
    void prepare(double sampleRate) noexcept {
        sampleRate_ = sampleRate;
        reset();
    }
    
    void reset() noexcept {
        std::fill(prevMagnitudes_.begin(), prevMagnitudes_.end(), 0.0f);
        std::fill(recentFluxes_.begin(), recentFluxes_.end(), 0.0f);
        fluxWriteIndex_ = 0;
        transientHoldCounter_ = 0;
        frameTransient_ = false;
    }
    
    // Process magnitude spectrum from FFT (called once per hop)
    // Returns true if this frame is transient
    bool processFrame(const float* magnitudes, int numBins) noexcept {
        numBins = std::min(numBins, kNumBins);
        
        // Calculate spectral flux (only positive differences)
        float flux = 0.0f;
        for (int i = 0; i < numBins; ++i) {
            float diff = magnitudes[i] - prevMagnitudes_[i];
            if (diff > 0.0f) {
                flux += diff;
            }
            prevMagnitudes_[i] = magnitudes[i];
        }
        
        // Store flux in circular buffer
        recentFluxes_[fluxWriteIndex_] = flux;
        fluxWriteIndex_ = (fluxWriteIndex_ + 1) % kLookbackFrames;
        
        // Calculate median flux (simple but RT-safe)
        std::array<float, kLookbackFrames> sortedFluxes = recentFluxes_;
        std::sort(sortedFluxes.begin(), sortedFluxes.end());
        float medianFlux = sortedFluxes[kLookbackFrames / 2];
        
        // Check if current flux exceeds threshold
        bool fluxTransient = (flux > medianFlux * kSpectralFluxMultiplier);
        
        // Update transient state with hold window
        if (fluxTransient) {
            transientHoldCounter_ = kTransientHoldFrames;
            frameTransient_ = true;
        } else if (transientHoldCounter_ > 0) {
            transientHoldCounter_--;
            frameTransient_ = true;
        } else {
            frameTransient_ = false;
        }
        
        return frameTransient_;
    }
    
    // Check if a specific partial should be treated as transient
    // Combines spectral flux detection with amplitude threshold
    bool isPartialTransient(float amplitude, int binIndex) const noexcept {
        // Amplitude threshold fallback
        if (amplitude >= kAmplitudeThreshold) {
            return true;
        }
        
        // Check if frame was marked transient by spectral flux
        if (frameTransient_ && binIndex >= 0 && binIndex < kNumBins) {
            // Could add per-bin transient tracking here if needed
            return true;
        }
        
        return false;
    }
    
    // Get current transient state
    bool isFrameTransient() const noexcept { return frameTransient_; }
    
    // Map FFT bin to nearest partial index (helper for integration)
    static int binToPartialIndex(int binIndex, int numPartials, int fftSize) noexcept {
        // Simple linear mapping - could be refined based on frequency scales
        return (binIndex * numPartials) / (fftSize / 2);
    }
    
private:
    double sampleRate_ = 44100.0;
    
    // Spectral flux detection state
    std::array<float, kNumBins> prevMagnitudes_;
    std::array<float, kLookbackFrames> recentFluxes_;
    int fluxWriteIndex_ = 0;
    
    // Transient state
    int transientHoldCounter_ = 0;
    bool frameTransient_ = false;
};

} // namespace scp