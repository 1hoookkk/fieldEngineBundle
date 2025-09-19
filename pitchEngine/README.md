# pitchEngine Pro

Hard‑tune you can perform through, with a **Style** control that adds depth and focus, plus a mysterious **Secret** mode for premium tone control.

## Build Instructions

1. **Prerequisites**:
   - Visual Studio 2022 (Windows) or Xcode (macOS)
   - CMake 3.21+
   - JUCE framework (included via local installation)

2. **Configure and Build**:
   ```bash
   cd pitchEngine
   cmake -S . -B build
   cmake --build build --config Release
   ```

3. **Find Built Plugin**:
   - VST3: `build/pitchEnginePro_artefacts/Release/VST3/`
   - AU (macOS): `build/pitchEnginePro_artefacts/Release/AU/`

## Parameters

**Core Parameters**:
- `key` - Musical key (C, C#, D, etc.)
- `scale` - Scale type (Chromatic, Major, Minor)
- `retuneMs` - Retune speed (1-200 ms)
- `strength` - Correction strength (0-100%)
- `formant` - Formant preservation (0-100%)
- `style` - **Secret sauce Z-plane morphing** (0-100%)

**Control Parameters**:
- `stabilizer` - Pitch stabilization (Off, Short, Mid, Long)
- `qualityMode` - Processing mode (Track, Print)
- `autoGain` - Automatic level matching (On/Off)
- `bypass` - Plugin bypass (On/Off)
- `secretMode` - **Premium "alternate path" mode** (On/Off)

## Architecture

### DSP Engine
- **ZPlaneStyle**: Core secret-sauce filter using embedded LUT JSON
  - 33-step morphing path with pole interpolation
  - 6-section biquad cascade (12-pole filtering)
  - Sample-rate aware θ scaling
  - Secret mode adds coefficient quantization, morph slew, and noise floor

### Plugin Structure
- **PluginProcessor**: 11-parameter APVTS with smoothing
- **PluginEditor**: Professional rotary knobs + combo boxes
- **Utility Classes**: OnePole, AutoGain, SibilantGuard
- **Stub Classes**: PitchTracker, Snapper, Shifter, Analyzer (ready for v1.1)

### Secret Mode Features
When **Secret = On**:
- Light coefficient quantization (fixed-grid math feel)
- ~6ms morph slew for continuous movement
- Tiny noise floor (−88 dBFS) to avoid sterile silence
- Maintains zero added latency

## File Structure

```
pitchEngine/
├── CMakeLists.txt              # Build configuration
├── README.md                   # This file
├── resources/
│   ├── zlut/
│   │   └── pitchEngine_zLUT_ref48k.json  # Z-plane LUT data
│   └── icons/
│       └── appIcon.png         # Plugin icon
└── source/
    ├── PluginProcessor.h/cpp   # Main plugin logic
    ├── PluginEditor.h/cpp      # User interface
    ├── dsp/
    │   ├── ZPlaneStyle.h/cpp   # Secret sauce engine
    │   ├── PitchTracker.h/cpp  # Stub for v1.1
    │   ├── Snapper.h/cpp       # Stub for v1.1
    │   ├── Shifter.h/cpp       # Stub for v1.1
    │   └── Analyzer.h/cpp      # Stub for v1.1
    └── util/
        ├── OnePole.h           # Simple low-pass filter
        ├── AutoGain.h          # Level matching utility
        └── SibilantGuard.h     # Sibilant detection utility
```

## Current Status ✅

**Core Implementation Complete**:
- ✅ Full JUCE CMake project structure
- ✅ Z-plane Style engine with LUT morphing
- ✅ All critical DSP bugs fixed (θ scaling, per-channel processing, static init)
- ✅ secretMode parameter with premium tone control
- ✅ Professional UI with 11 parameters
- ✅ Parameter smoothing for premium feel
- ✅ Embedded binary data (LUT JSON + icon)

**Ready for Testing**: Plugin should load in DAWs and Style parameter should produce audible morphing effects. secretMode adds subtle "classic feel" enhancements.

## Next Steps (v1.1)

- Implement pitch tracker (MPM/YIN hybrid)
- Add real-time pitch correction with Snapper + Shifter
- Build analyzer component with spectrum + pitch heatline
- Premium UI overhaul with design tokens
- A/B state management and bypass crossfading

## Build Notes

- Uses local JUCE installation at `../JUCE`
- Project version: 0.9.0
- Small build errors can be delegated to IDE/toolchain fixes
- Plugin formats: VST3, AU, AAX (when SDK available)

---

*"Flip Secret for that instant, classic feel."* ✨