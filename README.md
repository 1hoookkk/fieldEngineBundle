# FieldEngine VST3 Plugin Suite

**Advanced audio plugins with reverse-engineered EMU Z-plane filtering**

> *"Users hear the magic, never see the controls"*

## Overview

FieldEngine is a professional VST3 plugin suite that brings authentic EMU hardware character to modern DAWs. Built on reverse-engineered EMU Z-plane filtering algorithms, these plugins deliver premium vintage sound with streamlined modern workflows.

## Plugins

### 🎛️ morphEngine (Production Ready)
**Real-time filter morphing with authentic EMU Z-plane curves**

- **Status**: ✅ Built, installed, working VST3
- **Features**: Vowel/Bell/Low filter models, touchpad morph control, hardware-inspired UI
- **Unique**: Authentic EMU pole data with ZMF1 runtime loading
- **Design**: Compact 360×200px utility interface, TempleOS color scheme

### 🎵 pitchEngine Pro (In Development)
**Zero-latency pitch correction for performers and producers**

- **Status**: 🔧 Framework complete, DSP algorithms in progress
- **Features**: Track mode (0ms latency), Print mode (studio quality), Z-plane Style curves
- **Competitive**: Designed to beat MetaTune on feel, tone, and workflow
- **Target**: Professional studios and performing artists

### 🔮 Future Plugins
- Autotune with EMU character
- Hardware-modeled synthesizers
- Professional FX suite
- Rhythm and sequencing tools

## Technical Foundation

### Core Technologies
- **JUCE Framework**: Cross-platform audio plugin development
- **EMU Authentication**: Reverse-engineered hardware filtering algorithms
- **ZMF1 Format**: Embedded filter loading system
- **Real-time Safety**: Zero-allocation audio processing

### Supported Formats
- VST3 (primary target)
- AU (Audio Units)
- AAX (Pro Tools)
- Standalone applications

### Build System
- **CMake** with modern C++17
- **Pamplejuce** template for JUCE plugins
- **Cross-platform**: Windows, macOS, Linux

## Getting Started

### Prerequisites
- CMake 3.22+
- Modern C++ compiler (MSVC, GCC, Clang)
- JUCE dependencies

### Building
```bash
# Clone with submodules
git clone --recurse-submodules <repository-url>
cd fieldEngineBundle

# Configure build
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build morphEngine (production ready)
cmake --build build --target morphEngine

# Build pitchEngine Pro (in development)
cmake --build build --target pitchEngine
```

### Installation
Built plugins are located in:
- `build/morphEngine_artefacts/Release/VST3/`
- `build/pitchEngine_artefacts/Release/VST3/`

Copy `.vst3` bundles to your system's VST3 directory.

## Architecture

### EMU Z-plane System
The heart of FieldEngine is authentic EMU hardware modeling:

- **Vault Data**: Extracted from original EMU hardware ROMs
- **Pole Interpolation**: Mathematical reproduction of analog filter behavior
- **ZMF1 Format**: Compact binary format for embedded filter data
- **Runtime Loading**: Zero-allocation filter switching in audio thread

### Plugin Framework
- **Shared DSP**: Common filtering and processing components
- **Independent Builds**: Each plugin compiles separately
- **Professional UI**: Hardware-inspired interfaces with premium aesthetics
- **Parameter Management**: JUCE ValueTreeState for host automation

## Development Status

### morphEngine: Production Ready ✅
- Complete DSP implementation
- Professional UI design
- Successful DAW integration
- Performance validated

### pitchEngine Pro: Framework Complete 🔧
- JUCE plugin structure ready
- Parameter system implemented
- DSP components need algorithm completion
- UI design in progress

## Philosophy

**Premium Sound, Streamlined Workflow**

FieldEngine plugins prioritize musical results over technical complexity. Every control serves a clear musical purpose. The interface gets out of the way so artists can focus on creativity.

**Authentic Vintage Character**

Rather than modeling vintage gear behavior, we've reverse-engineered the actual algorithms. This delivers the authentic EMU sound that defined countless hit records.

**Modern Performance Standards**

Real-time safety, low CPU usage, and rock-solid stability for professional production environments.

## Contributing

This is a passion project focused on bringing premium vintage character to modern production. The codebase emphasizes:

- **Musical relevance** over feature completeness
- **Authentic algorithms** over approximations
- **Professional reliability** over experimental features

## License

[License information to be added]

---

*Built with JUCE • Powered by authentic EMU algorithms • Designed for professionals*
