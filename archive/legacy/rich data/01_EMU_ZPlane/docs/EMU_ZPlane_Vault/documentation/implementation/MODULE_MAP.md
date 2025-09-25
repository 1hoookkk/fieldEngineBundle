# Module Map (extracted from Unified_Curation_20250913_102843)

This map groups source files into practical buckets for JUCE VST3 plugin development.
Each file was copied from the curated bundle and is intended to be **dropped into** your plugin project,
with any necessary namespace/warning fixes.

> Provenance for each file is also recorded in `INDEX.csv`.

## Buckets
- **dsp/fft** — FFT/STFT helpers, windows, spectral blocks.
- **dsp/filters** — Biquad/IIR/FIR/SVF and related utils.
- **dsp/modulation** — LFOs, envelopes, modulation routing helpers.
- **synth/spectral** — Oscillators, wavetables, spectral synth/resynthesis pieces.
- **utils/concurrency** — Lock-free queues, ring buffers, RT-safe primitives.
- **utils/math** — Math helpers, SIMD/approx functions.
- **gui/components** — JUCE Components, LookAndFeel, editor bits.
- **utils/parameters** — APVTS/value tree helpers.
- **misc** — Unclassified but potentially useful bits.

If a bucket is empty, it simply means no matching files were discovered in that area.