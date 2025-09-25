# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

EnginePlugins recreates EMU hardware Z-plane morphing filters (Audity 2000, Xtreme Lead-1, Orbit, Planet Phatt) with T1/T2 morphing, cascaded biquads, and RT-safe DSP. Complete with EMU bank extraction tools and JUCE plugin integration.

**STATUS**: ✅ PRODUCTION-READY - Historic achievement: Complete EMU Z-plane morphing implementation with real audio generation

## Commands

```powershell
./scripts/build.ps1           # Build x3 tools, fieldEngine, or plugin
./scripts/extract-bank.ps1    # Extract EMU banks with acceptance gates
./scripts/test.ps1            # Validate build and extraction pipeline
```

## Architecture

**Core Components:**
- **fieldEngine/** - Z-plane morphing DSP engine (RT-safe, 6th/12th-order cascaded biquads)
- **tools/x3/** - EMU bank reverse engineering (E5P1 chunk extraction with acceptance gates)
- **JSON banks** - Effect-ready format with filter models, LFO, ENV, modulation matrix

**Filter Models:**
`ZP:HyperQ-12` (12th-order), `ZP:HyperQ-6` (6th-order), `ZP:PhaserForm`, `ZP:VocalMorph`

**Live Morphing Features:**
- Built-in LFO modulation (0.02-8Hz, sine wave)
- Envelope follower→morph modulation (5-80ms attack, 10-1000ms release)
- 7 APVTS parameters: drive, intensity, morph, autoMakeup, sectSat, satAmt, lfoRate/Depth
- Chunk-based RT-safe modulation processing (64-sample chunks)
- Fixed dry/wet polarity with fail-safe fallback

**Key Files:**
- `fieldEngine/Source/Core/DSPEngine.h:453-589` - Complete DSP engine with LFO and envelope follower
- `fieldEngine/juce/PluginProcessor.h:96-136` - Built-in modulation system (SimpleLFO, EnvMod)
- `fieldEngine/juce/PluginProcessor.cpp:173-296` - Live morphing processBlock with chunk-based modulation
- `tools/x3/tools/main.cpp` - CLI entry point for EMU bank extraction
- `fieldEngine/preset_loader_json.cpp` - JSON bank loading with EngineApply API

## Safety & Workflow

Git safety, build validation, and acceptance gate enforcement handled by hooks and scripts.
Context manifests provide detailed architecture per task.

## Sessions Behaviors

@CLAUDE.sessions.md

## Development Workflow

1. **Check existing patterns** in neighboring files before changes
2. **Use centralized scripts** - don't reinvent established workflows
3. **Test acceptance gates** - extraction must pass quality validation
4. **Preserve locality** - keep related code together vs over-abstracting