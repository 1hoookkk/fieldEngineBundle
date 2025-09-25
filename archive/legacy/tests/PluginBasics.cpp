#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <shared/EMUFilter.h>
#include <shared/AtomicOscillator.h>
#include <synth/FieldEngineSynthProcessor.h>

TEST_CASE ("sanity: one equals one", "[sanity]")
{
    REQUIRE (1 == 1);
}

TEST_CASE ("EMUFilterCore prepare/process smoke", "[dsp]")
{
    EMUFilterCore filter;
    filter.prepareToPlay(48000.0);
    filter.setFilterType(EMUFilterCore::LowPass);
    filter.setCutoffFrequency(1000.0f);
    filter.setResonance(0.5f);

    constexpr int N = 128;
    float buffer[N]{};
    buffer[0] = 1.0f; // impulse

    REQUIRE_NOTHROW(filter.processBlock(buffer, N));

    // Check output is finite and not NaN
    for (int i = 0; i < N; ++i)
        REQUIRE(std::isfinite(buffer[i]));
}

TEST_CASE ("EMUFilterCore LP Freq Response", "[dsp]")
{
    EMUFilterCore filter;
    filter.prepareToPlay(44100.0);
    filter.setFilterType(EMUFilterCore::LowPass);
    filter.setCutoffFrequency(500.0f);
    filter.setResonance(0.1f);

    constexpr int N = 256;
    float buffer[N]{};
    
    // Test with a sine wave below the cutoff
    for (int i = 0; i < N; ++i)
        buffer[i] = std::sin(2.0f * juce::MathConstants<float>::pi * 100.0f * i / 44100.0f);
    filter.processBlock(buffer, N);
    float energyBelow = 0.0f;
    for (int i = 0; i < N; ++i)
        energyBelow += buffer[i] * buffer[i];

    // Test with a sine wave above the cutoff
    for (int i = 0; i < N; ++i)
        buffer[i] = std::sin(2.0f * juce::MathConstants<float>::pi * 2000.0f * i / 44100.0f);
    filter.processBlock(buffer, N);
    float energyAbove = 0.0f;
    for (int i = 0; i < N; ++i)
        energyAbove += buffer[i] * buffer[i];

    REQUIRE(energyAbove < energyBelow);
}

TEST_CASE ("AtomicOscillator Frequency", "[dsp]")
{
    AtomicOscillator osc;
    osc.setSampleRate(44100.0f);
    osc.setFrequency(440.0f);
    osc.setTargetAmplitude(1.0f);

    constexpr int N = 44100;
    float buffer[N]{};
    for (int i = 0; i < N; ++i)
        buffer[i] = osc.generateSample();
        
    int zeroCrossings = 0;
    for (int i = 1; i < N; ++i) {
        if (buffer[i-1] < 0 && buffer[i] >= 0)
            zeroCrossings++;
    }
    
    REQUIRE(zeroCrossings == Catch::Approx(440).margin(1));
}

TEST_CASE ("FieldEngineSynthProcessor State Management", "[processor]")
{
    FieldEngineSynthProcessor proc1;
    auto& apvts1 = proc1.getAPVTS();

    // Set parameters to non-default values
    apvts1.getParameter("DETUNE")->setValueNotifyingHost(0.5f);
    apvts1.getParameter("CUTOFF")->setValueNotifyingHost(0.25f);
    apvts1.getParameter("RESONANCE")->setValueNotifyingHost(0.75f);
    apvts1.getParameter("ATTACK")->setValueNotifyingHost(0.1f);
    apvts1.getParameter("DECAY")->setValueNotifyingHost(0.2f);
    apvts1.getParameter("SUSTAIN")->setValueNotifyingHost(0.3f);
    apvts1.getParameter("RELEASE")->setValueNotifyingHost(0.4f);
    apvts1.getParameter("OUTPUT")->setValueNotifyingHost(0.6f);

    juce::MemoryBlock state;
    proc1.getStateInformation(state);

    FieldEngineSynthProcessor proc2;
    proc2.setStateInformation(state.getData(), (int)state.getSize());
    auto& apvts2 = proc2.getAPVTS();

    // Verify that parameters were restored
    REQUIRE(*apvts2.getRawParameterValue("DETUNE") == Catch::Approx(0.5f));
    REQUIRE(*apvts2.getRawParameterValue("CUTOFF") == Catch::Approx(0.25f));
    REQUIRE(*apvts2.getRawParameterValue("RESONANCE") == Catch::Approx(0.75f));
    REQUIRE(*apvts2.getRawParameterValue("ATTACK") == Catch::Approx(0.1f));
    REQUIRE(*apvts2.getRawParameterValue("DECAY") == Catch::Approx(0.2f));
    REQUIRE(*apvts2.getRawParameterValue("SUSTAIN") == Catch::Approx(0.3f));
    REQUIRE(*apvts2.getRawParameterValue("RELEASE") == Catch::Approx(0.4f));
    REQUIRE(*apvts2.getRawParameterValue("OUTPUT") == Catch::Approx(0.6f));
}