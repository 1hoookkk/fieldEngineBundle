#pragma once
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <complex>
#include <atomic>
#include <mutex>

// Real-time spectrum analyzer with pitch heatline for UI visualization
class Analyzer {
public:
    static constexpr int FFT_ORDER = 11;  // 2048 samples
    static constexpr int FFT_SIZE = 1 << FFT_ORDER;
    static constexpr int SPECTRUM_BINS = FFT_SIZE / 2;
    static constexpr int PITCH_HISTORY = 512;  // ~10 seconds @ 50 Hz update rate

    void prepare(double sampleRate, int numChannels) {
        fs = sampleRate;
        channels = juce::jmin(2, numChannels);

        // Initialize FFT
        fft = std::make_unique<juce::dsp::FFT>(FFT_ORDER);

        // Initialize buffers
        inputBuffer.resize(FFT_SIZE, 0.0f);
        fftData.resize(FFT_SIZE * 2, 0.0f);  // Real + imaginary
        spectrumMagnitudes.resize(SPECTRUM_BINS, 0.0f);

        // Initialize atomic spectrum data for UI
        atomicSpectrum = std::make_unique<std::atomic<float>[]>(SPECTRUM_BINS);
        for (int i = 0; i < SPECTRUM_BINS; ++i) {
            atomicSpectrum[i].store(0.0f, std::memory_order_relaxed);
        }

        // Initialize lock-free pitch history arrays
        atomicPitchHistory = std::make_unique<std::atomic<float>[]>(PITCH_HISTORY);
        atomicConfidenceHistory = std::make_unique<std::atomic<float>[]>(PITCH_HISTORY);
        for (int i = 0; i < PITCH_HISTORY; ++i) {
            atomicPitchHistory[i].store(0.0f, std::memory_order_relaxed);
            atomicConfidenceHistory[i].store(0.0f, std::memory_order_relaxed);
        }
        atomicHistoryIndex.store(0, std::memory_order_relaxed);

        // Processing state
        inputIndex = 0;
        samplesSinceLastUpdate = 0;

        // Update rate: ~50 Hz for smooth visualization
        updatePeriod = int(fs / 50.0);
    }

    void push(const float* samples, int numSamples) {
        if (!fft || numSamples <= 0) return;

        for (int i = 0; i < numSamples; ++i) {
            // Store sample in circular buffer
            inputBuffer[inputIndex] = samples[i];
            inputIndex = (inputIndex + 1) % FFT_SIZE;

            if (++samplesSinceLastUpdate >= updatePeriod) {
                processSpectrum();
                samplesSinceLastUpdate = 0;
            }
        }
    }

    // Get spectrum data for UI (thread-safe)
    void getSpectrumData(std::vector<float>& spectrum) {
        spectrum.resize(SPECTRUM_BINS);
        for (int i = 0; i < SPECTRUM_BINS; ++i) {
            spectrum[i] = atomicSpectrum[i].load(std::memory_order_relaxed);
        }
    }

    // Get pitch heatline data (lock-free, thread-safe)
    void getPitchHeatline(std::vector<float>& pitches, std::vector<float>& confidences) {
        const int currentIndex = atomicHistoryIndex.load(std::memory_order_acquire);

        pitches.resize(PITCH_HISTORY);
        confidences.resize(PITCH_HISTORY);

        // Copy in chronological order starting from oldest entry
        for (int i = 0; i < PITCH_HISTORY; ++i) {
            const int idx = (currentIndex + i) % PITCH_HISTORY;
            pitches[i] = atomicPitchHistory[idx].load(std::memory_order_relaxed);
            confidences[i] = atomicConfidenceHistory[idx].load(std::memory_order_relaxed);
        }
    }

    // Update pitch tracking data (lock-free, called from audio thread)
    void updatePitchData(float pitchHz, float confidence) {
        const int currentIndex = atomicHistoryIndex.load(std::memory_order_relaxed);
        const int nextIndex = (currentIndex + 1) % PITCH_HISTORY;

        // Update data first
        atomicPitchHistory[nextIndex].store(pitchHz, std::memory_order_relaxed);
        atomicConfidenceHistory[nextIndex].store(confidence, std::memory_order_relaxed);

        // Update index last with release semantics to ensure data is visible
        atomicHistoryIndex.store(nextIndex, std::memory_order_release);
    }

    // Get frequency for bin index
    float binToFrequency(int bin) const {
        return (bin * float(fs)) / (2.0f * SPECTRUM_BINS);
    }

    // Get bin index for frequency
    int frequencyToBin(float freq) const {
        return int((freq * 2.0f * SPECTRUM_BINS) / float(fs));
    }

private:
    double fs = 48000.0;
    int channels = 2;
    int updatePeriod = 960;  // ~50 Hz @ 48kHz

    // FFT processing
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> inputBuffer;
    std::vector<float> fftData;
    std::vector<float> spectrumMagnitudes;

    // Thread-safe spectrum data for UI (using raw array of atomics)
    std::unique_ptr<std::atomic<float>[]> atomicSpectrum;

    // Lock-free pitch history for heatline
    std::unique_ptr<std::atomic<float>[]> atomicPitchHistory;
    std::unique_ptr<std::atomic<float>[]> atomicConfidenceHistory;
    std::atomic<int> atomicHistoryIndex{0};

    // Processing state
    int inputIndex = 0;
    int samplesSinceLastUpdate = 0;

    void processSpectrum() {
        // Copy current window from circular buffer
        for (int i = 0; i < FFT_SIZE; ++i) {
            int readIndex = (inputIndex - FFT_SIZE + i + FFT_SIZE) % FFT_SIZE;
            fftData[i] = inputBuffer[readIndex] * getWindow(i);
            fftData[FFT_SIZE + i] = 0.0f;  // Clear imaginary part
        }

        // Perform FFT
        fft->performRealOnlyForwardTransform(fftData.data());

        // Calculate magnitudes with smoothing
        const float smoothing = 0.2f;  // Smooth animation
        for (int i = 0; i < SPECTRUM_BINS; ++i) {
            const float real = fftData[i];
            const float imag = fftData[FFT_SIZE + i];
            const float magnitude = std::sqrt(real * real + imag * imag);

            // Convert to dB with noise floor
            const float dB = 20.0f * std::log10(std::max(1e-6f, magnitude / FFT_SIZE));
            const float normalizedDb = juce::jlimit(0.0f, 1.0f, (dB + 60.0f) / 60.0f);  // -60 to 0 dB range

            // Smooth update
            const float currentValue = atomicSpectrum[i].load(std::memory_order_relaxed);
            const float newValue = currentValue + smoothing * (normalizedDb - currentValue);
            atomicSpectrum[i].store(newValue, std::memory_order_relaxed);
        }
    }

    // Hanning window for better frequency resolution
    float getWindow(int index) const {
        const float n = float(index) / (FFT_SIZE - 1);
        return 0.5f - 0.5f * std::cos(2.0f * juce::MathConstants<float>::pi * n);
    }
};