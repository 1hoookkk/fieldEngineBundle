# Architecture Overview

- **Audio Thread**: `processBlock` operates at high priority; keep it lock-free and allocation-free.
- **Message Thread**: UI components, parameter controls, preset loading; communicate via APVTS or lock-free queues.
- **DSP Blocks**: Prefer small, composable classes (Filter, FFT, Modulation) with `prepare`, `reset`, and `process` methods.
- **Memory**: Preallocate in `prepareToPlay`; reuse buffers and avoid STL operations that allocate.
- **Denormals**: Use `juce::ScopedNoDenormals` or set DAZ/FTZ.