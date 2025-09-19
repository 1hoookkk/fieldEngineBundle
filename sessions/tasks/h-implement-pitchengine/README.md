---
task: h-implement-pitchengine
branch: feature/implement-pitchengine
status: pending
created: 2025-09-19
modules: [pitchEngine, DSP, UI, CMake]
---

# pitchEngine JUCE VST3 Plugin Implementation

## Problem/Goal
Create a complete JUCE CMake VST3 plugin called "pitchEngine Pro" - a real-time hard-tuning plugin with surgical analysis and a secret **Style** macro powered by Z-plane LUT data. This is the foundation for a premium pitch correction plugin with unique morphing capabilities.

## Success Criteria
- [x] Complete pitchEngine directory structure with all required files
- [x] Working CMakeLists.txt with JUCE integration and binary data embedding
- [x] Move LUT JSON to resources/zlut/ directory
- [ ] **CRITICAL FIXES** identified by DSP tutor:
  - [ ] Fix θ sample-rate scaling inversion (48000.0/fsHost not fsHost/48000.0)
  - [ ] Fix per-channel IIR state processing (separate L/R contexts)
  - [ ] Remove static init from audio thread (make member variable)
  - [ ] Fix coefficients construction for JUCE compatibility
  - [ ] Add placeholder appIcon.png to prevent build failures
  - [ ] Add parameter smoothing for Style knob (premium feel)
- [ ] PluginProcessor with 10 parameters (key, scale, retuneMs, strength, formant, style, stabilizer, qualityMode, autoGain, bypass)
- [ ] Professional PluginEditor UI with knobs, dropdowns, and buttons
- [ ] ZPlaneStyle DSP engine with LUT-based Z-plane filtering
- [ ] Plugin builds successfully in Release configuration
- [ ] VST3 loads in DAW and Style parameter produces audible changes
- [ ] Parameter automation and state saving working

## Context Files
- pitchEngine_zLUT_ref48k.json # Existing Z-plane LUT data in root
- JUCE/ # Existing JUCE submodule for plugin framework
- CMakeLists.txt # Main project CMake for reference
- sessions/tasks/TEMPLATE.md # Task template structure

## User Notes
- Complete scaffold provided in detailed pitchEngine kit
- Focus on Style parameter as the unique differentiator
- Create foundation for future pitch engine implementation
- Follow JUCE best practices and cross-platform compatibility
- Stub out pitch tracking components for v1.1 expansion

## Work Log
- [2025-09-19] Started task, analyzing pitchEngine kit requirements and creating project structure