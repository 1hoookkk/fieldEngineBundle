#pragma once
#include <vector>
#include <cmath>
#include <complex>

/**
 * Simple Formant Shifter for robotic/gender-bending effects
 * Uses spectral envelope manipulation for throat modeling
 */
class FormantShifter {
public:
    void prepare(double sampleRate, int blockSize) {
        sr = sampleRate;
        maxBlockSize = blockSize;

        // Prepare FFT buffers
        fftSize = 2048; // Fixed FFT size for quality
        hopSize = fftSize / 4;

        inputBuffer.resize(fftSize, 0.0f);
        outputBuffer.resize(fftSize, 0.0f);
        window.resize(fftSize);

        // Generate Hann window
        for (int i = 0; i < fftSize; ++i) {
            window[i] = 0.5f - 0.5f * std::cos(2.0 * M_PI * i / (fftSize - 1));
        }

        writePos = 0;
        readPos = 0;
    }

    // Process with formant shift ratio (0.5 = higher voice, 2.0 = lower voice)
    void process(float* buffer, int numSamples, float shiftRatio) {
        // Bypass if neutral
        if (std::abs(shiftRatio - 1.0f) < 0.01f) return;

        // Simple comb filter approach for quick formant shift
        // This is much lighter than full spectral processing
        combFormantShift(buffer, numSamples, shiftRatio);
    }

private:
    double sr = 48000.0;
    int maxBlockSize = 512;
    int fftSize = 2048;
    int hopSize = 512;

    std::vector<float> inputBuffer;
    std::vector<float> outputBuffer;
    std::vector<float> window;

    int writePos = 0;
    int readPos = 0;

    // Lightweight comb filter formant shift
    void combFormantShift(float* buffer, int numSamples, float ratio) {
        // Delay line approach with variable delay for formant shift
        static std::vector<float> delayLine(8192, 0.0f);
        static int delayWritePos = 0;

        for (int n = 0; n < numSamples; ++n) {
            float input = buffer[n];

            // Write to delay line
            delayLine[delayWritePos] = input;

            // Calculate delay amounts for formant shifting
            float baseDelay = 200.0f / ratio; // Inverse relationship
            int delay1 = (int)(baseDelay * 0.7f);
            int delay2 = (int)(baseDelay * 1.3f);
            int delay3 = (int)(baseDelay * 2.1f);

            // Clamp delays
            delay1 = std::clamp(delay1, 1, 8000);
            delay2 = std::clamp(delay2, 1, 8000);
            delay3 = std::clamp(delay3, 1, 8000);

            // Read from delays with different taps
            int pos1 = (delayWritePos - delay1 + 8192) % 8192;
            int pos2 = (delayWritePos - delay2 + 8192) % 8192;
            int pos3 = (delayWritePos - delay3 + 8192) % 8192;

            float delayed1 = delayLine[pos1];
            float delayed2 = delayLine[pos2];
            float delayed3 = delayLine[pos3];

            // Mix original with formant-shifted signal
            float wet = (delayed1 * 0.5f + delayed2 * 0.3f + delayed3 * 0.2f);

            // Crossfade based on shift amount
            float mixAmount = std::min(1.0f, std::abs(ratio - 1.0f) * 2.0f);
            buffer[n] = input * (1.0f - mixAmount) + wet * mixAmount;

            // Update write position
            delayWritePos = (delayWritePos + 1) % 8192;
        }
    }
};