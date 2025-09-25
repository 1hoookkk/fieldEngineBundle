# ZPlaneStyle DSP Component CLAUDE.md

## Purpose
Core Z-plane style morphing DSP engine with research-based stability improvements.

## Narrative Summary
ZPlaneStyle implements the core DSP processing for EMU Z-plane filter morphing with significant stability and quality improvements from recent DSP research. Features include corrected sample-rate scaling, Schur triangle stability projection, crossfading for smooth parameter transitions, and energy-based overload protection. The component processes embedded JSON LUT data to generate stable biquad cascades with authentic EMU character.

## Key Files
- `ZPlaneStyle.cpp:1-409` - Main DSP implementation
- `ZPlaneStyle.h` - Class definition and interface
- `Analyzer.cpp` - Real-time spectrum analysis
- `PitchTracker.cpp` - Pitch detection and tracking
- `Shifter.cpp` - Pitch shifting algorithms
- `Snapper.cpp` - Pitch quantization to scales

## DSP Architecture
### Biquad Cascade Structure
- 6-section all-pole biquad cascade
- Pole-ordered processing (low-Q to high-Q)
- Per-section unity normalization
- Cascade-wide gain compensation

### LUT Processing
- Embedded JSON tables: `BinaryData::pitchEngine_zLUT_ref48k_json`
- Interpolated pole positioning from reference data
- Smooth mapping with safety caps (0-85% range)
- Smoothstep easing for natural transitions

## Critical DSP Fixes (Research Findings)
### 1. Corrected Theta Scaling (Line 117)
**Problem**: Sample-rate scaling was inverted
**Fix**: `theta = thRef * (48000.0 / fsHost)` (was reversed)
**Impact**: Proper frequency mapping across sample rates

### 2. Pole Radius Limits (Lines 129-142)
**Implementation**: Sample-rate scaled radius bounds
- Base limits: 0.996-0.997 at 44.1kHz
- Scaled bounds: `rMin = pow(0.996, 44100/fsHost)`
- Soft limiting with K=4e-4 parameter
**Impact**: Prevents Q explosions while preserving character

### 3. Schur Triangle Stability (Lines 54-67)
**Method**: `projectToStableRegion(a1, a2)`
- Triangle bounds: |a2| < 1, |a1| < 1 + a2
- 2e-6 epsilon safety margin
- Character-preserving projection
**Impact**: Guaranteed stability without sonic artifacts

### 4. Matched-Z Sample Rate Conversion (Lines 124-126)
**Formula**: `r = pow(r, fsRef / fsHost)`
- Preserves bandwidth in Hz across sample rates
- Reference frequency: 48kHz
- Maintains filter character at all rates

## Parameter Processing
### Smoothing System
- 0.3ms time constant for snappy morphs
- Exponential smoothing with alpha coefficient
- Large jump detection for crossfading triggers
- Previous state tracking for change detection

### Crossfading Implementation (Lines 274-408)
- Dual filter paths (main + background)
- Cosine crossfade curves for smooth transitions
- 1024-sample crossfade length
- Automatic filter swapping after completion

## Quality Modes
### Secret Mode Features
- Coefficient quantization: 20-bit fixed-point feel
- Dither injection: ≈-88 dBFS noise floor
- Enhanced character processing

### Energy Protection
- Per-section peak tracking with exponential decay
- Gentle limiting at 12.04 dB threshold (kMaxEnergy = 16.0)
- Energy decay constant: 0.999 (kEnergyDecay)
- Last-resort protection without pumping

## Processing Flow
1. **Initialization**: Build coefficient tables from JSON LUT
2. **Parameter Smoothing**: 0.3ms time constant updates
3. **Crossfade Detection**: Monitor for large parameter jumps
4. **Coefficient Generation**: Interpolated poles → biquad coefficients
5. **Stability Projection**: Schur triangle enforcement
6. **Cascade Processing**: 6-section biquad chain
7. **Energy Monitoring**: Per-section overload protection

## Performance Optimizations
### Real-Time Safety
- ScopedNoDenormals protection
- Member-based initialization (not static)
- Atomic operations for thread safety
- Zero-allocation processing paths

### Coefficient Caching
- Dual filter banks (main/background)
- Shared coefficients between L/R channels
- Background updates during crossfades
- Efficient filter swapping

## Mathematical Constants
- `kLargeJumpThreshold = 0.02f` - Crossfade trigger threshold
- `kCrossfadeLength = 1024` - Crossfade duration in samples
- `kEnergyDecay = 0.999f` - Energy tracker time constant
- `kMaxEnergy = 16.0f` - Energy limiter threshold (≈12dB)

## Integration Points
### Consumes
- JSON LUT data from embedded binary
- Real-time parameter updates
- Sample rate and buffer size information

### Provides
- Processed audio output
- Real-time coefficient frames
- Energy monitoring data
- Crossfade state information

## Testing Requirements
- Sample rate invariance (44.1/48/96 kHz)
- Parameter smoothness (no clicks/pops)
- Stability under extreme parameters
- Energy limiting effectiveness
- Crossfade artifact elimination

## Key Patterns
- Pole sorting by Q-factor for cascade stability (lines 148-150)
- Background coefficient updates during crossfades (line 281)
- Per-section normalization with cascade compensation (lines 153-201)
- Energy-based limiting as final safety measure (lines 398-405)

## Related Documentation
- `../PluginProcessor.cpp` - Plugin integration layer
- `../../../plugins/morphengine/CLAUDE.md` - MorphEngine implementation
- `../../../../DSP_VERIFICATION.md` - Testing methodology