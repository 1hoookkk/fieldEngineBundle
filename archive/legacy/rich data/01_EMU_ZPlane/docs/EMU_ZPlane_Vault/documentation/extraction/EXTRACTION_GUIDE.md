# Extraction Guide

This guide shows how to port code from `extracted/` into a JUCE VST3 plugin.

## 1) Pick a module
- **Filters** → `extracted/dsp/filters/` (e.g., Biquad/IIR/SVF)
- **FFT/Spectral** → `extracted/dsp/fft/` & `extracted/synth/spectral/`
- **Modulation** → `extracted/dsp/modulation/` (LFO/ADSR)
- **Concurrency** → `extracted/utils/concurrency/` (lock-free FIFOs for audio/MIDI exchange)
- **Parameters** → `extracted/utils/parameters/`

## 2) Add sources to CMake
```cmake
target_sources(JUCEPluginTemplate PRIVATE ../../extracted/dsp/filters/Biquad.cpp)
target_include_directories(JUCEPluginTemplate PRIVATE ../../extracted)
```

## 3) Wire up in `PluginProcessor`
```cpp
void JUCEPluginTemplateAudioProcessor::prepareToPlay (double sr, int spb) {
    // example: filter.prepare (sr, spb);
}
void JUCEPluginTemplateAudioProcessor::processBlock (juce::AudioBuffer<float>& buf, juce::MidiBuffer&) {
    // example: filter.process (buf);
}
```

## 4) Parameters
Use APVTS and smoothers to avoid zipper noise. See `Source/Parameters.h`.

## 5) Testing
- Add a small standalone app build or use JUCE's UnitTest framework.
- For audio validation: impulse and sine sweep through your module; null tests against a reference where possible.