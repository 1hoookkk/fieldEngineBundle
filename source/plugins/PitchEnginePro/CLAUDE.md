# PitchEnginePro CLAUDE.md

## Purpose
Professional pitch correction plugin using Z-plane style morphing for authentic vocal processing.

## Narrative Summary
PitchEnginePro implements advanced pitch correction with Z-plane style morphing for natural-sounding vocal tuning. The plugin features sophisticated parameter smoothing, bypass handling, and a "secret mode" for enhanced character. Built on the proven ZPlaneStyle DSP engine with real-time processing optimizations and smooth parameter transitions.

## Key Files
- `PluginProcessor.cpp:18-213` - Main plugin processor
- `PluginProcessor.h` - Plugin class definition  
- `PluginEditor.cpp` - Plugin UI implementation
- `PluginEditor.h` - UI class definition
- `dsp/ZPlaneStyle.cpp:1-409` - Core DSP processing
- `dsp/ZPlaneStyle.h` - DSP class definition

## Parameters
### Core Pitch Parameters
- `key` - Target key: C, C#, D, D#, E, F, F#, G, G#, A, A#, B (default A)
- `scale` - Scale type: Chromatic(0), Major(1), Minor(2) (default Minor)
- `retuneMs` - Retune speed (1-200ms, default 12ms)
- `strength` - Correction strength (0-100%, default 100%)
- `formant` - Formant preservation (0-100%, default 80%)

### Style & Character
- `style` - Z-plane style morphing (0-100%, default 35%)
- `secretMode` - Enhanced character mode (boolean, default false)
- `autoGain` - Automatic gain compensation (boolean, default true)

### Quality & Control
- `qualityMode` - Processing quality: Track(0), Print(1) (default Track)
- `stabilizer` - Pitch stabilization: Off(0), Short(1), Mid(2), Long(3)
- `bypass` - Plugin bypass (boolean, default false)

## Parameter Smoothing
### Smoothing Time Constants
- Style: 50ms (kStyleSmoothSeconds)
- Strength: 100ms (kStrengthSmoothSeconds) 
- Retune: 200ms (kRetuneSmoothSeconds)
- Bypass: 5ms fade (kBypassFadeSeconds)

### Implementation Details
- Per-sample parameter smoothing for artifact-free transitions
- Cached parameter pointers for RT performance
- Atomic parameter loading with fallback values
- Bypass crossfading with dry signal preservation

## DSP Architecture
### Z-Plane Style Engine
- 6-section biquad cascade from EMU Z-plane tables
- Sample-rate invariant processing (44.1-96 kHz)
- Pole ordering by Q-factor for cascade stability
- Schur triangle stability projection
- Matched-Z sample rate conversion

### Key DSP Innovations (from research findings)
- **Corrected theta scaling**: Fixed sample-rate scaling inversion (ZPlaneStyle.cpp:117)
- **Pole radius limits**: 0.996-0.997 range scaled per sample rate (lines 129-142)
- **Stability projection**: Schur triangle bounds enforcement (lines 54-67)
- **Crossfade system**: Smooth parameter transitions with dual filter paths (lines 274-408)
- **Energy monitoring**: Per-section overload protection (lines 389-406)

## Secret Mode Features
When enabled (`secretMode = true`):
- Coefficient quantization for "fixed-grid math feel" (lines 175-181)
- Tiny dither injection to prevent sterile silence (lines 292-302)
- Enhanced character processing

## Processing Quality
### Stability Measures
- Denormal protection (FTZ/DAZ) enforcement
- Per-section energy monitoring with gentle limiting
- Soft pole radius limiting to prevent Q explosions
- Cascade gain normalization with 18dB headroom

### Performance Optimizations
- Member-based initialization flags (not static)
- Background filter crossfading for smooth transitions
- Lightweight energy tracking with exponential decay
- Optimized coefficient sharing between channels

## Bypass Implementation
- Smooth crossfading with preserved dry signal
- Per-sample mixing with linear interpolation
- Proper state management during bypass transitions
- Zero-latency dry path preservation

## Integration Points
### Consumes
- Binary LUT data: `BinaryData::pitchEngine_zLUT_ref48k_json`
- JUCE parameter automation system
- Real-time audio processing thread

### Provides
- VST3/AU plugin interface
- Real-time pitch correction processing
- Parameter automation support
- Professional-grade audio quality

## Configuration
No special environment variables required.

Build dependencies:
- JUCE framework
- Embedded JSON LUT tables
- C++20 standard library

## Key Patterns
- Atomic parameter caching for RT safety (see cacheParameterPointers:34-41)
- Smooth bypass transitions with dry preservation (see processBlock:119-135)
- Background filter swapping for artifact-free parameter changes (see ZPlaneStyle:359-371)
- Energy-based overload protection as last resort (see lines 389-406)

## Related Documentation
- `plugins/morphengine/CLAUDE.md` - Related morphing engine
- `DSP_VERIFICATION.md` - DSP testing and validation
- `dsp/ZPlaneStyle.h` - Core DSP component documentation