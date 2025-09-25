# Claude Reading Kit — Audio DSP & EMU Z‑Plane Vault
Generated: 2025-09-20 10:51 UTC

This package is organized for quick understanding by a language model.

## Folders
- **01_EMU_ZPlane/** — Z‑Plane filter specs, extracted library code, and build files.
- **02_SpectralResearch/** — FFT/STFT and spectral processing components and notes.
- **03_Source_ProductLayer/** — Product and UI docs plus JUCE-facing code that would host the DSP.
- **99_reports/** — Machine-generated indices to speed up orientation.

Within each collection:
- `docs/` → Markdown, text, PDFs, CSVs
- `src/` → Code and configs (C/C++/JUCE, JSON/YAML/TOML, project files like `.jucer`, `CMakeLists.txt`)
- `assets/` → Images and audio examples
- `other/` → Anything that didn’t fit the above

## High‑signal entry points
- `99_reports/archive_overview.md` — curated overview and quick pointers.
- `99_reports/code_highlights.csv` — files likely defining processors, filters, FFT utils, and Z‑Plane modules.
- `01_EMU_ZPlane/src/emu_extracted/CMakeLists.txt` — build entry for the Z‑Plane library.

## Suggested instructions (paste into your prompt)
1. **Skim the overview:** Read `99_reports/archive_overview.md` to understand the repo layout and main modules.
2. **Map the DSP surface:** Use `99_reports/code_highlights.csv` to jump to processors/filters/FFT utilities and summarize their responsibilities.
3. **Reconstruct the Z‑Plane path:** From `01_EMU_ZPlane/docs` and `01_EMU_ZPlane/src`, outline how the Z‑Plane filters are implemented (data structures, coefficient flow, morphing), and produce a concise developer guide for integration into a JUCE `AudioProcessor`.
4. **Connect to product:** From `03_Source_ProductLayer/docs`, extract requirements for SpectralCanvas/PaintEngine and propose the minimal plugin that demonstrates both Z‑Plane filtering and spectral masking (“MaskSnapshot”).

## Questions for analysis (so Claude prioritizes usefully)
- What is the minimal compile graph for the Z‑Plane library? Which files are core?
- Where is spectral masking performed, and how would you ensure RT‑safety?
- What parameters (cutoff, resonance, morph index) should be exposed for a Z‑Plane demo?
- What test audio or fixtures exist to validate correctness/regressions?
- What UI affordances are required for PaintEngine/SpectralCanvas MVP?

## License & provenance
Original paths and sources are preserved in `MANIFEST.json` (maps every file to its new location).
