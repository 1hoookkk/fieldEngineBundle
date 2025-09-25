# CLAUDE.md

## Project Overview

JUCE-based VST3 plugin suite with EMU Z-plane filtering (autotune, synth, FX, rhythm).
Built in C++23+ with JUCE 8.0.10 and custom DSP libraries.
Claude Code is the sole director for synthesizing 100% of the code.

---

## Commands

### Configure & Build

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=20
cmake --build build -j
```

### Targets

**Plugin**:

```bash
cmake --build build --target morphEngine -j
```

**Tests**:

```bash
cmake --build build --target test_basic_engine test_sr_invariance test_null_when_intensity_zero -j
```

Run tests (from `build/`):

```bash
ctest --output-on-failure
```

or explicitly:

```bash
./test_basic_engine && ./test_sr_invariance && ./test_null_when_intensity_zero
```

---

## Options

* `-DBUILD_MORPHENGINE=ON` (default)
* `-DBUILD_SECRETSAUCE=OFF`
* `-DENABLE_LTO=ON` (link-time optimization, Release)
* `-DENABLE_ASAN=ON` / `-DENABLE_UBSAN=ON` (sanitizers for Debug/Dev)
* `-DWARNING_AS_ERROR=ON` (strict CI)

---

## File Boundaries

**Safe to edit**:

* `/plugins/morphengine/`
* `/libs/emu/`
* `/libs/pitchengine_dsp/`
* `/qa/tests/`
* `/source/plugins/`

**Never touch**:

* `/JUCE/`
* `/build/`
* `/.git/`
* `/assets/`
* `/rich data/`
* `/archive/`

⚠ Rule: Never modify third-party vendored code or build output directories.

---

## Coding Standards

**Language**: Modern C++23+. JUCE 8.0.10 patterns for class/file naming (`PluginProcessor.cpp`).
**Formatting**: Always apply repo `.clang-format` before commit.

**Naming**:

* Types/classes → `PascalCase`
* Functions → `camelCase`
* Constants/macros → `UPPER_SNAKE_CASE`

---

## Architecture

* **Core DSP** = EMU Z-plane engine (6 biquads, SR remap, equal-power mix).
* Block-latched values; smoothing handled in plugin, not engine.
* Realtime paths: allocation-free, branch-light.

**Principles**:

* Locality > abstraction
* Readability > cleverness
* Consistency across files

---

## DSP Invariants & Tests (MUST pass before commit)

* Null/transparent path: `intensity=0 → pass-through`
* Sample-rate invariance: works at 44.1/48/96 kHz
* Morph continuity: no pops; smooth transitions

Rule: Always configure, build, and run all tests successfully before commit.

---

## Quality Gates (Non-negotiable)

✅ Build must succeed (Release or CI config)
✅ All DSP tests must pass
✅ `clang-format` applied
✅ Commit message: imperative, concise, scoped; include what/why/side effects
✅ Self-verification enforced: Claude must check its own work before marking task complete

---

## Permissions & Modes

* **Auto-accept mode ON**: All edits, writes, and allowed bash commands executed without prompts
* **No popups**: Never request confirmation
* **Plan Mode**: Required for config/system file changes
* **File boundaries enforced**: Auto-accept applies only to safe zones

---

## Agents

See `@AGENTS.md`

---

## Behavioral Rules

See `@CLAUDE.sessions.md`

---
