Template Plugin (Pamplejuce)

Overview
- Purpose: Skeleton for a plugin compatible with the Field Engine bundle.
- How to use: Duplicate this folder (or run the helper script) and fill in `plugin.json` and `src/` with your implementation.

Implement
- Pamplejuce: Edit `pamplejuce.yml` with your plugin/company details.
- Generate project: Run Pamplejuce to create JUCE/CMake files in this folder.
- DSP link: The generated CMake will include `cmake/LinkPitchEngineDSP.cmake` which links `libs/pitchengine_dsp`.
- Engine entry: Provide the runtime entry at the `plugin.json:entry` path for your host.

Conventions
- Name: lowercase with dashes (e.g., `my-awesome-plugin`).
- Version: semantic versioning (e.g., `0.1.0`).
- Compatibility: set `engines.fieldEngine` to the engine version range you support.

Test
- Add unit tests under `tests/` using the test toolchain your host project uses.

Distribute
- Keep your plugin self-contained under this folder.
- Validate `plugin.json` against `../plugin.schema.json` before publishing.
