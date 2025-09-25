#include <catch2/benchmark/catch_benchmark_all.hpp>
#include <catch2/catch_test_macros.hpp>
#include <shared/EMUFilter.h>
#include <shared/ZPlaneFilter.h>
#include <synth/FieldEngineSynthProcessor.h>
#include <pitchengine_dsp/AuthenticEMUZPlane.h>
#include <random>

namespace {
    void fillWhiteNoise(std::vector<float>& buffer) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        for (auto& sample : buffer) {
            sample = dist(gen);
        }
    }
}

TEST_CASE ("Boot performance")
{
    BENCHMARK_ADVANCED ("EMUFilterCore prepare")
    (Catch::Benchmark::Chronometer meter)
    {
        meter.measure ([&] {
            EMUFilterCore f; f.prepareToPlay(48000.0); return 0; });
    };

    BENCHMARK_ADVANCED ("EMUFilterCore process 128 samples")
    (Catch::Benchmark::Chronometer meter)
    {
        std::vector<float> buf(128, 0.0f);
        EMUFilterCore f; f.prepareToPlay(48000.0); f.setCutoffFrequency(2000.0f);
        fillWhiteNoise(buf);
        meter.measure ([&] (int) {
            f.processBlock(buf.data(), (int) buf.size());
            return buf[0];
        });
    };
}

TEST_CASE ("DSP Performance")
{
    BENCHMARK_ADVANCED ("ZPlaneFilter process 128 samples")
    (Catch::Benchmark::Chronometer meter)
    {
        std::vector<float> left(128);
        std::vector<float> right(128);
        fillWhiteNoise(left);
        fillWhiteNoise(right);

        fe::ZPlaneFilter f;
        f.prepare(48000.0, 128);
        f.setMorph(0.5f);
        f.updateCoefficientsBlock();
        
        meter.measure ([&] (int) {
            f.processBlock(left.data(), right.data(), 128);
            return left[0];
        });
    };
    
    BENCHMARK_ADVANCED ("FieldEngineSynthProcessor 8 voices")
    (Catch::Benchmark::Chronometer meter)
    {
        juce::AudioBuffer<float> buffer(2, 512);
        juce::MidiBuffer midi;
        
        FieldEngineSynthProcessor p;
        p.prepareToPlay(48000.0, 512);
        
        // Turn on all voices
        for (int i = 0; i < 8; ++i)
            midi.addEvent(juce::MidiMessage::noteOn(1, 60 + i, 1.0f), 0);
        
        meter.measure ([&] (int) {
            p.processBlock(buffer, midi);
            return buffer.getSample(0,0);
        });
    };

    BENCHMARK_ADVANCED ("AuthenticEMUZPlane process 128 samples")
    (Catch::Benchmark::Chronometer meter)
    {
        juce::AudioBuffer<float> buffer(2, 128);
        std::vector<float> leftNoise(128), rightNoise(128);
        fillWhiteNoise(leftNoise);
        fillWhiteNoise(rightNoise);

        // Copy noise to buffer
        for (int i = 0; i < 128; ++i) {
            buffer.setSample(0, i, leftNoise[i]);
            buffer.setSample(1, i, rightNoise[i]);
        }

        AuthenticEMUZPlane emu;
        emu.prepareToPlay(48000.0);
        emu.setMorphPosition(0.5f);
        emu.setIntensity(0.3f);       // Default from plugin
        emu.setDrive(3.0f);           // Default from plugin
        emu.setSectionSaturation(0.35f); // Default from plugin

        meter.measure ([&] (int) {
            emu.process(buffer);
            return buffer.getSample(0, 0);
        });
    };
}
