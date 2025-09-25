# MorphEngine CLAUDE.md

## Purpose
EMU Z-plane morphing filter plugin with style-based presets and motion modulation.

## Narrative Summary
MorphEngine implements authentic EMU Z-plane filtering with three style variants (Air, Liquid, Punch) mapped to different pole configurations. The plugin applies equal-power crossfading between dry and wet signals with optional quality modes for print/track workflows. Features smooth parameter transitions, safe mode limits, and integrated motion modulation via LFO or DAW sync.

## Key Files
- `src/MorphEngineAudioProcessor.cpp:39-695` - Main plugin processor
- `src/MorphEngineAudioProcessor.h` - Plugin class definition
- `src/PremiumMorphUI.cpp` - Premium GUI implementation
- `src/PremiumMorphUI.h` - Premium GUI class

## Parameters
### Core Parameters
- `zplane.morph` - Morphing position (0-100%, default 28%)
- `zplane.resonance` - Filter resonance/intensity (0-100%, default 18%)
- `style.variant` - Filter style: Air(0), Liquid(1), Punch(2)
- `style.mix` - Dry/wet mix (0-100%, default 35%)
- `tilt.brightness` - Spectral tilt (-6 to +6 dB, default 0)
- `drive.db` - Input drive (0-12 dB, default 0)
- `hardness` - Section saturation (0-100%, default 20%)
- `output.trim` - Output level (-12 to +12 dB, default 0)

### Motion Modulation
- `motion.source` - LFO source: Off(0), LFO Sync(1), LFO Hz(2)
- `motion.depth` - Modulation depth (0-100%, default 0%)
- `motion.hz` - Rate in Hz (0.05-8.0 Hz, default 0.5)
- `motion.division` - Sync division (1 to 1/32, default 1/4)
- `motion.retrig` - Retrigger on transport start

### Quality & Safety
- `quality.mode` - Processing quality: Track(0), Print(1)
- `safe.mode` - Parameter limiting for stability

## Style Mappings
### Air (MorphPair 0)
- Bright, airy character
- Higher frequency emphasis
- 12 factory presets (Freeze, Glassline, Whisper, etc.)

### Liquid (MorphPair 1)
- Smooth, flowing character
- Mid-frequency focus
- 12 factory presets (Drift, Shimmer, Chrome, etc.)

### Punch (MorphPair 2)
- Dark, punchy character
- Lower frequency emphasis
- 12 factory presets (Glue, Snap, Thump, etc.)

## Safe Mode Limits
When enabled, parameters are constrained for stability:
- Morph: 8-85% (from 0-100%)
- Resonance: 0-50% (from 0-100%)
- Drive: ≤2 dB (from 0-12 dB)
- Hardness: ≤40% (from 0-100%)
- Tilt: ±2 dB (from ±6 dB)
- Mix: 0-65% (from 0-100%)

## Quality Modes
### Track Mode
- No oversampling (1x)
- Lower latency
- Optimized for real-time tracking

### Print Mode
- 2x oversampling with anti-aliasing
- Higher latency but improved quality
- Anti-aliasing lowpass at 18kHz/0.45*Fs

## DSP Architecture
- Core: AuthenticEMUZPlane engine (6 biquad sections)
- Sample rates: 44.1/48/96 kHz with SR-invariant processing
- Block-latched parameter smoothing (20-200ms time constants)
- Equal-power dry/wet mixing with sin/cos curves
- Optional print-quality oversampling with IIR anti-aliasing

## Motion Modulation Implementation
- Maps to EMU engine morph position
- DAW-sync with musical divisions (1 to 1/32, dotted, triplet)
- Free-running Hz mode (0.05-8.0 Hz range)
- Optional retrigger on transport start
- Exponential LFO shape for musical feel

## Integration Points
### Consumes
- EMU Z-plane coefficient tables
- JUCE parameter automation
- DAW transport information (for sync)
- Binary LUT data (pitchEngine_zLUT_ref48k.json)

### Provides
- VST3/AU plugin interface
- Filter coefficient frames for visualization
- Real-time spectrum analysis data
- Factory preset system (36 presets)

## Configuration
Required environment variables:
- `FE_SIMPLE_UI=1` - Forces terminal UI (fallback mode)

Optional build flags:
- `LOAD_AUDITY_MODEL_PACK` - Runtime model loading

## Key Patterns
- Parameter smoothing handled in plugin, not engine (see MorphEngineAudioProcessor.cpp:156-173)
- Async quality changes to avoid RT allocations (see processBlock:332-344)
- Thread-safe coefficient publishing for UI (see publishFilterFrame:473-483)
- Factory preset system with per-style organization (see initializeFactoryPresets:501-543)

## Related Documentation
- `source/plugins/PitchEnginePro/CLAUDE.md` - Related pitch engine
- `DSP_VERIFICATION.md` - DSP testing documentation
- `QUICK_WINS_IMPLEMENTED.md` - Implementation notes