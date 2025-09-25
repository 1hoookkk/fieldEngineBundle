# pitchEngine CLAUDE.md

## Purpose
Advanced pitch correction VST3 plugin with surgical oversampling and authentic EMU Z-plane filtering.

## Narrative Summary
The pitchEngine service implements a professional pitch correction plugin using reverse-engineered EMU Z-plane filtering technology. Recent enhancements include surgical oversampling architecture that applies linear filtering at base sample rate while oversampling only the nonlinear components for optimal performance. The plugin operates in dual modes: Track (low-latency, 1x) for real-time monitoring and Print (high-quality, 2x/4x oversampling) for final processing.

## Key Files
- `source/PluginProcessor.h:7` - Main processor with OversampledEmu integration
- `source/PluginProcessor.cpp:126-138` - Real-time latency reporting and mode switching
- `source/dsp/OversampledEmu.h` - JUCE-native surgical oversampling wrapper
- `source/dsp/ZPlaneHelpers.h` - Authentic EMU pole interpolation utilities
- `source/dsp/AuthenticEMUZPlane.h:134-159` - Linear/nonlinear processing separation
- `source/ui/HeaderBar.h:6-10` - Latency display UI component
- `CMakeLists.txt` - VST3 build configuration with SharedCode linking

## Core Architecture
### Surgical Oversampling Pattern
- Linear filtering: Always at base sample rate (48kHz)
- Nonlinear processing: Oversampled only when needed (96kHz/192kHz in Print mode)
- Zero-copy audio buffer management with non-owning JUCE AudioBuffer views

### Mode-Based Operation
- **Track Mode**: 1x processing, zero latency, real-time monitoring
- **Print Mode**: 2x IIR or 4x FIR oversampling, plugin delay compensation (PDC)
- Dynamic mode switching with automatic latency reporting to host

## API Integration Points
### Consumes
- JUCE DSP: `juce::dsp::Oversampling<float>` for anti-aliasing filters
- SharedCode: Common DSP utilities and EMU data tables
- Host: Plugin delay compensation (PDC) in Print mode

### Provides
- VST3/AU/AAX: Standard plugin formats
- Real-time latency reporting: `setLatencySamples()` host notifications
- Parameter automation: Morph position, intensity, drive controls

## Configuration
### Oversampling Settings
- IIR filters: Minimal latency (polyphase, near-zero samples)
- FIR filters: Linear phase, higher latency (~2304 samples at 48kHz)
- Maximum block size: 2048 samples with dynamic buffer allocation

### Build Requirements
- JUCE 7.0+: Audio processor framework
- C++17: Modern language features
- CMake 3.21+: Cross-platform build system

## Key Implementation Patterns
### EmuAdapter Pattern (OversampledEmu.h:8-35)
Minimal adapter providing zero-allocation wrappers for AuthenticEMUZPlane processing with automatic sample rate scaling for oversampling contexts.

### Surgical Processing (OversampledEmu.h:69-96)
Two-stage processing: linear filtering at base rate, then nonlinear oversampling only, avoiding unnecessary upsampling of linear components.

### Dynamic Latency Management (PluginProcessor.cpp:126-138)
Real-time mode detection with automatic PDC reporting to host when switching between Track and Print modes.

## Related Documentation
- `source/benchmarks/PitchEngineBenchmarks.h:181-206` - Latency measurement tests
- `CMakeLists.txt` - Build configuration and VST3 parameters
- `enginesSecretSauce/CLAUDE.md` - Related EMU processing service