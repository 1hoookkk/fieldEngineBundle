
# HardTune (JUCE Plugin)

A clean, commercial hard-tuning audio plugin built on your `pitchengine_dsp` library. Targets the classic hard‑quantized correction effect with near‑instant retune, scale snapping, optional high‑quality print mode, **and an EMU Z‑plane Color macro** for that signature sheen.

## Features

- **Color (EMU Z‑plane)**: a single macro that adds the EMU gloss (intensity, mild drive, section saturation)
- **Two Modes**: `Track` (zero‑latency PSOLA) and `Print` (higher quality)
- **Retune Speed**: 0..1 (1 = instant, classic hard tune)
- **Correction Amount**: blend from subtle to fully hard‑tuned
- **Key / Scale**: Chromatic, Major, Minor
- **Input Type**: pre-sets detector range (Soprano..Bass)
- **Formant Shift / Throat**: placeholders to wire to your formant DSP
- **Sibilant Guard**: reduces artifacts on S, T, F sounds
- **Mix**: dry/wet

## Build

```
mkdir build && cd build
cmake -DJUCE_DIR=/path/to/JUCE ..
cmake --build . --config Release
```

JUCE is expected via `-DJUCE_DIR` (folder containing JUCE's top-level CMakeLists.txt).

## Notes

- The **Color** control maps to `AuthenticEMUZPlane` parameters: intensity, drive (0..+3 dB), and mild section saturation. It is applied **to the wet path only**.
- Replace/extend `formant`/`throat` once your formant module is ready.
