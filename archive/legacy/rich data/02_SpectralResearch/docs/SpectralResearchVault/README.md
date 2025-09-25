# SpectralResearchVault - Curated DSP Components

## Overview
This vault contains **verified, working DSP components** extracted from the SpectralCanvasPro research codebase. Each component has been validated for completeness, quality, and usefulness.

## Curation Criteria
✅ **Complete implementation** (no stubs or TODOs)  
✅ **Focused purpose** (does one thing well)  
✅ **Professional quality** (proper RT-safety, threading, etc.)  
✅ **Documented interface** (clear parameters and usage)  
✅ **Under 300 lines** (maintainable size)

## Verified Components

### 🎛️ Analog Modeling
- **CEM3389Filter** (285 lines) - EMU Audity analog filter with paint integration
- **MorphFilter** (60 lines) - State Variable Filter with smooth morphing
- **Z-PlaneSnapshot** (127 lines) - Clean Z-plane implementation with morphing

### 🔧 Core Utilities  
- **AtomicOscillator** (43 lines) - Lock-free RT-safe oscillator

### 🌈 Spectral Processing (Extracted)
- **SpectralBlur** - Gaussian kernel convolution for frequency blur
- **SpectralFreeze** - Freeze specific frequency bands
- **Paint Integration** - Map paint gestures to parameters

## Rejected Components
❌ **CDPSpectralEngine** (1149 lines) - Over-engineered kitchen sink  
❌ **EMURomplerEngine** - Stub implementation with TODOs  
❌ **UI Frameworks** - Not DSP-focused  
❌ **Analytics/Telemetry** - Plugin bloat

## Integration Strategy
Each component is designed to integrate into:
1. **SpectralCanvasLite** - Add analog character + Z-plane morphing
2. **New Specialized Plugins** - Use components as building blocks
3. **ResonanceEngine** - Enhance existing Z-plane implementation

## Usage
See individual component directories for integration examples and dependencies.

---
*Curated from 16,000+ source files down to ~20 verified components*
*Date: 2025-09-13*