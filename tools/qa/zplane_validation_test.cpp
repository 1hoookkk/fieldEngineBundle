#include <iostream>
#include <vector>
#include <cmath>
#include <cassert>
#include "libs/emu/qa/ZPlane_STFTHarness.h"
#include "libs/emu/engines/AuthenticEMUEngine.h"
#include "libs/emu/api/StaticShapeBank.h"
#include "libs/emu/wrappers/OversampledEngine.h"

class ZPlaneValidator {
public:
    ZPlaneValidator() : shapes(48000.0f), engine(shapes) {
        harness.prepare(48000.0, 10, 256); // 1024-point FFT, 256 log bins
    }

    bool runAllTests() {
        std::cout << "=== Z-Plane Engine Validation Tests ===" << std::endl;

        bool allPassed = true;
        allPassed &= testNullCondition();
        allPassed &= testSampleRateInvariance();
        allPassed &= testOversamplingBenefit();

        if (allPassed) {
            std::cout << "\n✓ All tests PASSED" << std::endl;
        } else {
            std::cout << "\n✗ Some tests FAILED" << std::endl;
        }

        return allPassed;
    }

private:
    StaticShapeBank shapes;
    AuthenticEMUEngine engine;
    OversampledEngine osEngine;
    zplane::qa::STFTHarness harness;

    // Test 1: Null test - intensity=0 should produce no change
    bool testNullCondition() {
        std::cout << "\n1. Testing null condition (intensity=0)..." << std::endl;

        // Generate test signal
        const int N = 1024;
        std::vector<float> input(N), output(N);
        generateTestSignal(input.data(), N, 440.0f); // A4
        std::copy(input.begin(), input.end(), output.begin());

        // Setup engine with null parameters
        engine.prepare(48000.0, N, 1);
        osEngine.prepare(48000.0, 1, OversampledEngine::Mode::Off_1x);
        osEngine.setMaxBlock(N);

        ZPlaneParams nullParams{};
        nullParams.intensity = 0.0f; // NULL condition
        nullParams.morph = 0.5f;
        nullParams.driveDb = 0.0f;
        nullParams.sat = 0.0f;

        engine.setParams(nullParams);

        // Process
        juce::AudioBuffer<float> buffer(1, N);
        std::copy(output.begin(), output.end(), buffer.getWritePointer(0));

        if (!engine.isEffectivelyBypassed()) {
            osEngine.process(engine, buffer, N);
        }

        // Compare input vs output
        float maxDiff = 0.0f;
        for (int i = 0; i < N; ++i) {
            const float diff = std::abs(input[i] - buffer.getSample(0, i));
            maxDiff = std::max(maxDiff, diff);
        }

        const bool passed = maxDiff < 1e-6f;
        std::cout << "   Max difference: " << maxDiff << " (should be < 1e-6)" << std::endl;
        std::cout << "   Result: " << (passed ? "PASS" : "FAIL") << std::endl;

        return passed;
    }

    // Test 2: Sample rate invariance - same preset should behave similarly across sample rates
    bool testSampleRateInvariance() {
        std::cout << "\n2. Testing sample rate invariance..." << std::endl;

        const int N = 1024;
        std::vector<float> signal48k(N), signal44k(N), signal96k(N);
        generateTestSignal(signal48k.data(), N, 440.0f);
        std::copy(signal48k.begin(), signal48k.end(), signal44k.begin());
        std::copy(signal48k.begin(), signal48k.end(), signal96k.begin());

        // Test parameters
        ZPlaneParams testParams{};
        testParams.intensity = 0.7f;
        testParams.morph = 0.3f;
        testParams.driveDb = 3.0f;
        testParams.sat = 0.1f;

        std::vector<float> spectrum48k, spectrum44k, spectrum96k;

        // Test at 48 kHz
        testAtSampleRate(48000.0, signal48k.data(), N, testParams, spectrum48k);

        // Test at 44.1 kHz
        testAtSampleRate(44100.0, signal44k.data(), N, testParams, spectrum44k);

        // Test at 96 kHz
        testAtSampleRate(96000.0, signal96k.data(), N, testParams, spectrum96k);

        // Compare spectral differences (should be within a few dB)
        const float diff48_44 = harness.l2diff(spectrum48k, spectrum44k);
        const float diff48_96 = harness.l2diff(spectrum48k, spectrum96k);

        const bool passed = (diff48_44 < 5.0f) && (diff48_96 < 5.0f);
        std::cout << "   48k vs 44.1k difference: " << diff48_44 << " dB RMS" << std::endl;
        std::cout << "   48k vs 96k difference: " << diff48_96 << " dB RMS" << std::endl;
        std::cout << "   Result: " << (passed ? "PASS" : "FAIL") << std::endl;

        return passed;
    }

    // Test 3: Oversampling benefit - Print mode should reduce THD vs Track mode
    bool testOversamplingBenefit() {
        std::cout << "\n3. Testing oversampling benefit..." << std::endl;

        const int N = 1024;
        std::vector<float> signal(N);
        generateTestSignal(signal.data(), N, 440.0f);

        // Test with drive up to create nonlinear distortion
        ZPlaneParams driveParams{};
        driveParams.intensity = 0.8f;
        driveParams.morph = 0.5f;
        driveParams.driveDb = 9.0f; // High drive
        driveParams.sat = 0.3f;     // High saturation

        std::vector<float> spectrumTrack, spectrumPrint;

        // Test Track mode (1x)
        testWithOversamplingMode(signal.data(), N, driveParams,
                               OversampledEngine::Mode::Off_1x, spectrumTrack);

        // Test Print mode (2x IIR)
        testWithOversamplingMode(signal.data(), N, driveParams,
                               OversampledEngine::Mode::OS2_IIR, spectrumPrint);

        // Measure THD improvement (should be lower with oversampling)
        float thdTrack = measureTHD(spectrumTrack);
        float thdPrint = measureTHD(spectrumPrint);

        const float thdImprovement = thdTrack - thdPrint;
        const bool passed = thdImprovement > 3.0f; // At least 3dB improvement

        std::cout << "   Track mode THD: " << thdTrack << " dB" << std::endl;
        std::cout << "   Print mode THD: " << thdPrint << " dB" << std::endl;
        std::cout << "   THD improvement: " << thdImprovement << " dB (should be > 3dB)" << std::endl;
        std::cout << "   Result: " << (passed ? "PASS" : "FAIL") << std::endl;

        return passed;
    }

    void generateTestSignal(float* output, int N, float frequency) {
        const float omega = 2.0f * M_PI * frequency / 48000.0f;
        for (int i = 0; i < N; ++i) {
            output[i] = 0.5f * std::sin(omega * i);
        }
    }

    void testAtSampleRate(double sampleRate, float* signal, int N,
                         const ZPlaneParams& params, std::vector<float>& spectrum) {
        engine.prepare(sampleRate, N, 1);
        osEngine.prepare(sampleRate, 1, OversampledEngine::Mode::Off_1x);
        osEngine.setMaxBlock(N);

        engine.setParams(params);

        juce::AudioBuffer<float> buffer(1, N);
        std::copy(signal, signal + N, buffer.getWritePointer(0));

        if (!engine.isEffectivelyBypassed()) {
            osEngine.process(engine, buffer, N);
        }

        harness.analyze(buffer.getReadPointer(0), N, spectrum);
    }

    void testWithOversamplingMode(float* signal, int N, const ZPlaneParams& params,
                                OversampledEngine::Mode mode, std::vector<float>& spectrum) {
        engine.prepare(48000.0, N, 1);
        osEngine.prepare(48000.0, 1, mode);
        osEngine.setMaxBlock(N);

        engine.setParams(params);

        juce::AudioBuffer<float> buffer(1, N);
        std::copy(signal, signal + N, buffer.getWritePointer(0));

        if (!engine.isEffectivelyBypassed()) {
            osEngine.process(engine, buffer, N);
        }

        harness.analyze(buffer.getReadPointer(0), N, spectrum);
    }

    float measureTHD(const std::vector<float>& spectrum) {
        // Simple THD measurement: ratio of fundamental to harmonics
        if (spectrum.size() < 32) return -60.0f;

        // Find fundamental peak (should be around bin corresponding to 440 Hz)
        int fundamentalBin = 8; // Approximate for 440 Hz in log spectrum
        float fundamentalLevel = spectrum[fundamentalBin];

        // Sum harmonic content (higher frequency bins)
        float harmonicSum = 0.0f;
        for (int i = fundamentalBin + 2; i < std::min(64, (int)spectrum.size()); ++i) {
            harmonicSum += std::pow(10.0f, spectrum[i] / 20.0f); // Convert dB to linear
        }

        if (harmonicSum < 1e-9f) return -60.0f;

        const float thdDb = 20.0f * std::log10f(harmonicSum / std::pow(10.0f, fundamentalLevel / 20.0f));
        return thdDb;
    }
};

int main() {
    try {
        ZPlaneValidator validator;
        const bool success = validator.runAllTests();
        return success ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}