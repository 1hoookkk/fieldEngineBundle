# EMU Test Plugin - Usage Guide

## What it is
A minimal JUCE plugin that uses the authentic EMU Z-plane engine to test the extracted shape data. This plugin provides a simple interface to hear how the authentic EMU morphing filters sound.

## How to build
```bash
cmake --build build --config Release --target EMUTest
```

## Plugin location
The VST3 plugin is created at:
```
build/EMUTest_artefacts/Release/VST3/EMU Test.vst3
```

## Parameters
- **Morph Pair**: Choose between "Vowel Pair", "Bell Pair", or "Low Pair" (the authentic shape pairs)
- **Morph**: 0.0 to 1.0 - morphs between the A and B shapes in the selected pair
- **Intensity**: 0.0 to 1.0 - controls filter intensity
- **Drive**: -20dB to +20dB - drive/gain before the filter
- **Saturation**: 0.0 to 1.0 - saturation amount
- **LFO Rate**: 0.0 to 10.0 Hz - LFO rate for morph modulation
- **LFO Depth**: 0.0 to 1.0 - LFO modulation depth
- **Auto Makeup**: Automatically compensates for gain changes
- **Bypass**: Bypass the effect

## How to test
1. Load the plugin in your DAW
2. Start with:
   - Morph Pair: "Vowel Pair"
   - Morph: 0.5
   - Intensity: 0.6
   - Drive: 0dB
   - Saturation: 0.0
3. Play some audio and sweep the Morph parameter to hear the authentic EMU morphing
4. Try different Morph Pairs to hear different character types
5. Add some Drive and Saturation for more character

## What you're hearing
- **Vowel Pair**: Morphs between vowel formants (Ae to Oo)
- **Bell Pair**: Morphs between bright metallic bell tones
- **Low Pair**: Morphs between punchy low-frequency resonant chains

The plugin uses the authentic EMU shape data extracted from the JSON files, compiled into the `EMUAuthenticTables.h` header.

## Technical details
- Uses `AuthenticEMUEngine` with 6 sections (12th order)
- No oversampling (simplified for testing)
- Generic JUCE editor (basic sliders)
- Real-time parameter updates
