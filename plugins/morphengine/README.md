MorphEngine Plugin

Overview
- This is the new plugin-packaged MorphEngine living under `plugins/morphengine`.
- It uses the Pamplejuce workflow and links against `libs/pitchengine_dsp` via a local CMake fragment.

How to build (Pamplejuce)
1) Edit `pamplejuce.yml` (name, company, codes, formats).
2) Run Pamplejuce in this folder to generate CMake/JUCE project files.
3) The generated CMake includes `cmake/LinkPitchEngineDSP.cmake` to link `libs/pitchengine_dsp`.

Engine Integration
- Implement the host entry at the path in `plugin.json:entry` (default `src/index`).
- Keep runtime-facing code within this folder; do not touch legacy folders.

Validation
- Validate `plugin.json` against `../plugin.schema.json`.

