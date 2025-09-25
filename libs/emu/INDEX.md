## EMU Z‑Plane Modular Engine: Architectural Overview

### Layers and responsibilities
- **API**
  - [`api/IZPlaneEngine.h`](api/IZPlaneEngine.h): realtime engine interface (prepare/reset/linear/nonlinear, params, SR)
  - [`api/IShapeBank.h`](api/IShapeBank.h): readonly shape source (exposes models/pairs and reference sample rate)
  - [`api/ZPlaneParams.h`](api/ZPlaneParams.h): parameter bundle, incl. calibration hooks (radiusGamma, postTilt, driveHardness)
- **Core DSP**
  - [`core/BiquadCascade.h`](core/BiquadCascade.h): 6 biquad sections (12th‑order); band‑pass numerator, clamps
  - [`core/ZPoleMath.h`](core/ZPoleMath.h): shortest‑path θ and proper bilinear z@fsRef → s → z@fsProc remap
  - [`core/NonlinearStage.h`](core/NonlinearStage.h): drive/saturation (tanh hardness)
  - [`core/SimpleTilt.h`](core/SimpleTilt.h): lightweight post‑tilt (dB/oct) for bank matching
  - [`core/VintageDAC.h`](core/VintageDAC.h): optional 18‑bit quantize with TPDF dither (print vibe)
- **Engine**
  - [`engines/AuthenticEMUEngine.h`](engines/AuthenticEMUEngine.h): authentic pole morphing; 6th/12th switch via `sectionsActive`
- **Wrappers**
  - [`wrappers/OversampledEngine.h`](wrappers/OversampledEngine.h): OS island around nonlinear only (1×/2× IIR/4× FIR)
- **Banks & Adapters**
  - [`api/StaticShapeBank.h`](api/StaticShapeBank.h): compiled tables shim; reports `getReferenceSampleRate()`
  - [`api/JsonShapeBank.h`](api/JsonShapeBank.h): loads `audity_shapes_*_48k.json`; pair ids (e.g., vowel/bell/low)
  - [`adapter/ZPlaneAdapter_FromAuthentic.h`](adapter/ZPlaneAdapter_FromAuthentic.h): arrays/JSON → `zplane::Model` vector
- **QA**
  - [`qa/ZPlane_STFTHarness.h`](qa/ZPlane_STFTHarness.h): 1k‑point STFT harness for spectral A/B sanity

### Processing flow (per block)
1. **Params in**: `ZPlaneParams` (morph, intensity, driveDb, sat, radiusGamma, postTiltDbPerOct, driveHardness)
2. **Morph**: linear radius + shortest‑path angle
3. **Calibrate**: apply `radiusGamma` on radius (pre‑remap)
4. **SR remap**: bilinear from bank ref SR → processing SR for each pole
5. **Coeffs**: band‑pass numerator per section; clamp `a1/a2`
6. **Linear cascade**: run first `sectionsActive ∈ {3,6}` sections at base rate
7. **Nonlinear island**: oversampled (if enabled) drive/saturation only
8. **Post**: subtle spectral tilt → optional vintage DAC (18‑bit + TPDF)

### Realtime guarantees
- No heap allocations in audio; OS buffers pre‑allocated via `setMaxBlock()`
- `isEffectivelyBypassed()` fast path when intensity≈0, drive≈1, sat≈0, lfoDepth≈0
- Latency reported by `OversampledEngine::getLatencySamples()`

### Sample‑rate invariance
- Banks declare `sampleRateRef` (JSON) or use ctor‑provided refSR; poles remapped per block using bilinear transform so formants track across 44.1/48/96 kHz

### Filter order & cost
- `setSectionsActive(3|6)` selects 6th/12th order; `getApproxVoiceCost()` hints host (1 vs 2)

### Minimal usage (host side)
```cpp
// construct once
StaticShapeBank shapes(48000.0f);
auto emu = std::make_unique<AuthenticEMUEngine>(shapes);
OversampledEngine os;

// prepare
emu->setSectionsActive(6);
emu->prepare(sampleRate, blockSize, numCh);
os.prepare(sampleRate, numCh, OversampledEngine::Mode::Off_1x);
os.setMaxBlock(blockSize);

// per block (wet branch)
ZPlaneParams p{}; /* fill from APVTS */
emu->setParams(p);
if (!emu->isEffectivelyBypassed())
    os.process(*emu, wetStereo, wetStereo.getNumSamples());
```

### Bank integration tips
- Pair ids (e.g., `vowel_pair`, `bell_pair`, `low_pair`) map A/B models; JSON bank exposes `sampleRateRef`
- Per‑bank trims to nail the last 5–10%: `radiusGamma`, `postTiltDbPerOct`, `driveHardness`

### QA checklist
- Null: intensity=0 → wet‑solo nulls (latency aligned)
- SR: same preset at 44.1/48/96 → formant peaks align (within cents)
- OS: THD drops in Print vs Track with drive/sat up
