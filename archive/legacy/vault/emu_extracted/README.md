EMU Z-Plane bundle — README (short)
----------------------------------
Purpose: reproduce EMU z-plane filter bank from extracted shapes.

Key concepts:
- Each "shape" = 6 complex pole pairs stored as 12 floats [r,theta] (r ~ 0..1, theta radians).
- Engine: interpolate shapeA->shapeB by morph (0..1), scale radius by intensity, convert each conjugate-pair to biquad (a1,a2) and a normalized numerator.
- Implementation files:
  - EMUFilter.h : authentic tables
  - AuthenticEMUZPlane.cpp : conversion & interpolation code (use verbatim)
  - ZPlaneFilter.h : core Z-plane mathematics
  - emu_zplane_api.h : minimal C ABI – recommended entry point for LLM
- Test: run sweep/impulse and compare IRs for A/B parity.

Files:
- emu_zplane_api.h : C API entry point (start here)
- manifest.json : file map and examples (LLM navigation)
- index.csv : symbol->line navigation for LLMs
- shapes/ : authentic EMU filter shapes (A/B morph pairs)
- banks/ : extracted bank data (Orbit3, Planet Phatt, Xtreme Lead)
- AuthenticEMUZPlane.cpp : core morphing engine
- EMUFilter.h/.cpp : filter implementation
- ZPlaneFilter.h : mathematics
- ZPlaneTables.h : lookup tables

Usage:
1. Include emu_zplane_api.h
2. Call emu_create(sampleRate, blockSize)
3. Load shapes with emu_set_shapeA/B()
4. Set morph/intensity with emu_set_*()
5. Process audio with emu_process_separate()

Units:
- r (radius): 0.0-1.0, must be < 1.0 for stability
- theta (angle): radians, 0 to 2π
- morph: 0.0=shapeA, 1.0=shapeB
- intensity: 0.0-1.0, scales radius/Q
- drive: dB, input gain
- saturation: 0.0-1.0, per-section amount

Real-time rules:
- Update coefficients once per block (expensive)
- No file I/O on audio thread
- Use smoothers for zipper-free parameters
- Clamp radii < 1.0 to avoid instability