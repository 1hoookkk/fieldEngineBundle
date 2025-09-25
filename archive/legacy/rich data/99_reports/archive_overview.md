# Archive Overview

## EMU_ZPlane_Vault - 1.zip

### Docs & Notes

- EMU_ZPlane_Vault/documentation/extraction/EXTRACTION_GUIDE.md — Extraction Guide
- EMU_ZPlane_Vault/documentation/extraction/HANDOFF_EMU_INTEGRATION.md — 🎯 EMU Bank Browser Integration - Session Handoff
- EMU_ZPlane_Vault/documentation/hardware_specs/EMU_AUDITY_2000_TECHNICAL_SPECS.md — EMU Audity 2000 Technical Specifications
- EMU_ZPlane_Vault/documentation/implementation/ARCHITECTURE_OVERVIEW.md — Architecture Overview
- EMU_ZPlane_Vault/documentation/implementation/MODULE_MAP.md — Module Map (extracted from Unified_Curation_20250913_102843)
- EMU_ZPlane_Vault/documentation/implementation/PORTING_CHECKLIST.md — Porting Checklist
- EMU_ZPlane_Vault/documentation/implementation/Z_PLANE_FILTER_IMPLEMENTATION_SPEC.md — Z-Plane Filter Implementation Specification
- EMU_ZPlane_Vault/documentation/research/RESEARCH_SOURCES.md — EMU Audity 2000 Research Sources
- EMU_ZPlane_Vault/README.md — EMU Z-Plane Filter Collection
### Config/JSON

- EMU_ZPlane_Vault/AUTHENTIC/shapes/audity_shapes_A_48k.json — JSON keys: sampleRateRef, shapes
- EMU_ZPlane_Vault/AUTHENTIC/shapes/audity_shapes_B_48k.json — JSON keys: sampleRateRef, shapes
- EMU_ZPlane_Vault/banks/orbit3/Orbit-3_comprehensive.json — JSON keys: meta, presets
- EMU_ZPlane_Vault/banks/planet_phatt/Planet_Phatt_comprehensive.json — JSON keys: meta, presets
- EMU_ZPlane_Vault/banks/proteus/Proteus1_fixed.json — JSON keys: meta, presets
- EMU_ZPlane_Vault/banks/proteus/ProteusX_Composer.json — JSON keys: meta, presets
- EMU_ZPlane_Vault/manifest.json — JSON keys: version, name, description, source, sampleRates, format, extractionDate, filterOrder

## Source - 2.zip

### Docs & Notes

- Source/assets/sprite_index.txt — sprite_index.txt
- Source/Core/MaskSnapshotDemo.md — MaskSnapshot Demo - RT-Safe Spectral Masking
- Source/docs/AI_Mockup_Prompts.md — SPECTRAL CANVAS PRO - AI Mockup Generation Prompts
- Source/docs/BEATMAKER_UI_STRATEGY.md — SoundCanvas Beatmaker UI Strategy
- Source/docs/CLAUDE.md — CLAUDE.md
- Source/docs/DEVJOURNAL.md — ARTEFACT Development Log
- Source/docs/INTEGRATION_COMPLETE.md — SoundCanvas PaintEngine Integration - COMPLETE ✅
- Source/docs/PaintEngine_README.md — PaintEngine - Real-Time Audio Painting System
- Source/docs/plan.md — SpectralCanvas Development Plan & Status
- Source/docs/README.md — README.md
### Config/JSON

- Source/assets/manifest.json — JSON keys: version, description, frames, panels, sprites, layout_constants, color_palette

## SpectralResearchVault.zip

### Docs & Notes

- SpectralResearchVault/PluginConcepts/VintageCanvas/README.md — VintageCanvas - Paint-Controlled Analog Processor
- SpectralResearchVault/README.md — SpectralResearchVault - Curated DSP Components

## emu_extracted - 1.zip

### Docs & Notes

- emu_extracted/CMakeLists.txt — EMU Z-Plane Library
- emu_extracted/README.md — README.md
### Config/JSON

- emu_extracted/banks/Orbit3_Authentic.json — JSON keys: meta, presets
- emu_extracted/banks/PlanetPhatt_Authentic.json — JSON keys: meta, presets
- emu_extracted/banks/XtremeLead1_Authentic.json — JSON keys: meta, presets
- emu_extracted/manifest.json — JSON keys: project, version, sampleRateRef, entry_header, files, examples, shape_pairs, units

## Frequent terms (rough signal of themes)

include, juce, float, pragma, once, void, audio, const, atomic, with, class, sampleRate, JuceHeader, public, memory, Core, namespace, double, static, Audio, constexpr, SpectralCanvas, File, safe, cmath, Spectral, paint, time, Description, default, spectral, thread, array, return, Copyright, Systems, vector, PaintEngine, size, uint