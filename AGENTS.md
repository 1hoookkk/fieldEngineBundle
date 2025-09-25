# Repository Guidelines

## Project Structure & Module Organization
- Root: CMake (Pamplejuce) project. JUCE is a subdirectory (`JUCE/`).
- Plugins: `plugins/morphengine/` (active), other templates in `plugins/_template/`.
- DSP Libraries: `libs/emu` (core EMU engine), `libs/pitchengine_dsp/`.
- Morph plugin sources: `morphEngine/source/`.
- Tests: `qa/tests/` (small executables for DSP checks).
- Assets & data: `assets/`, `rich data/` (reference extraction, not required at runtime).

## Build, Test, and Development Commands
- Configure (Release):
  - `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`
- Build the plugin:
  - `cmake --build build --target morphEngine -j`
- Build all tests:
  - `cmake --build build --target test_basic_engine test_sr_invariance test_null_when_intensity_zero`
- Run tests (from build dir):
  - `./test_basic_engine && ./test_sr_invariance && ./test_null_when_intensity_zero`
- Useful CMake options:
  - `-DBUILD_MORPHENGINE=ON` (default), `-DBUILD_SECRETSAUCE=OFF`.

## Coding Style & Naming Conventions
- Language: Modern C++ (C++17+), JUCE patterns for classes/files (e.g., `PluginProcessor.cpp`).
- Formatting: `clang-format` with repo `.clang-format`. Run before committing.
- Conventions:
  - Classes/Types: `PascalCase`; functions/methods: `camelCase`.
  - Constants/macros: `UPPER_SNAKE_CASE`.
  - File names: match primary class (e.g., `MorphFilter.h/.cpp`).

## Testing Guidelines
- Framework: lightweight CMake executables under `qa/tests` for DSP invariants.
- What to test:
  - Null/transparent paths (e.g., intensity=0 → pass‑through).
  - Sample‑rate invariance (44.1/48/96 kHz consistency).
  - Morph continuity (no pops, smooth transitions).
- Naming: `test_<topic>.cpp`. Keep tests deterministic and fast (<1s).

## Commit & Pull Request Guidelines
- Commits: imperative, concise, scoped (e.g., "Fix mix crossfade orientation").
- Include: what changed, why, and any side effects.
- PRs must include:
  - Clear summary, motivation, and testing notes (commands run, results).
  - Screenshots or audio notes if UI/sonic behavior changed.
  - Link related issues/tasks. Keep diffs focused and minimal.

## Architecture Notes
- Core processing uses a curated EMU z‑plane engine (6 biquads, SR remap, equal‑power mix).
- Smoothing occurs in the plugin; the engine expects block‑latched values.
- Keep real‑time paths allocation‑free and branch‑light.

## Specialized Agents (JUCE/VST3 Focus)

### 1. context-gathering (JUCE/DSP)
- **Purpose**: Reads plugin anatomy (Processor/Editor), EMU engine, libs/, and tests to produce Context Manifest
- **Scope**: Signal flow, parameters, SR handling, smoothing contracts
- **Allowed**: Read/Grep/Glob only; no edits
- **Output**: `docs/context/morphengine_context.md`
- **When**: Creating new task or when context feels thin

### 2. code-review (DSP & JUCE)
- **Purpose**: Reviews changed .cpp/.h for realtime safety, parameter smoothing, thread correctness, JUCE patterns
- **Scope**: No allocations in audio, formatting compliance, architecture adherence
- **Allowed**: Read; suggest diffs; no write
- **Output**: `ai-reviews/<branch>/<file>.review.md`
- **When**: Before commit; "one agent, one job" keeps reviews crisp

### 3. test-writer (DSP invariants)
- **Purpose**: Generates/updates qa/tests/ for null path, SR invariance, morph continuity
- **Scope**: Edge cases, performance smoke tests
- **Allowed**: Edit only in `/qa/tests/`
- **Output**: Updated tests + `qa/REPORT.md` summary
- **When**: Adding new DSP functionality or fixing test gaps

### 4. cmake/builder
- **Purpose**: Runs configure/build; captures errors; proposes minimal fixes
- **Scope**: Build system, dependencies, compilation issues
- **Allowed**: Bash(cmake:*, ctest:*, make:*, ninja:*), Read build logs
- **Output**: `build/BUILD_NOTES.md`
- **When**: Build failures or CMake configuration changes

### 5. service-documentation
- **Purpose**: Syncs /docs/ and CLAUDE.md sections when conventions or commands change
- **Scope**: Documentation drift, API changes, workflow updates
- **Allowed**: Edit documentation files only
- **Output**: Updated docs + short changelog
- **When**: After service changes or convention updates

### 6. dsp-profiler (analysis only)
- **Purpose**: Scans for hotspots (branches in audio loop, denormals, avoidable std:: operations)
- **Scope**: Performance analysis, optimization recommendations
- **Allowed**: Read-only analysis
- **Output**: `docs/perf_hotspots.md`
- **When**: Performance investigation or optimization phases

## Agent Principles
- **Delegate heavy work**: Let agents handle file-heavy operations for speed
- **Be specific**: Give agents clear context and goals
- **One agent, one job**: Don't combine responsibilities
- **Parallel execution**: Run read-heavy jobs in parallel to maximize efficiency
- **Narrow scope**: Keep each agent focused on its domain expertise
