# Faust + Foleys GUI Magic Integration Guide

This document explains how to integrate Faust DSP and Foleys GUI Magic into the FieldEngine project.

## Overview

- **Faust**: Functional DSP language that generates optimized C++ code
- **Foleys GUI Magic**: XML-based JUCE GUI framework with live editing

## Files Created

### Faust Integration
```
source/faust/
├── zplane_morph.dsp          # Faust DSP definition
├── FaustZPlaneProcessor.h    # JUCE wrapper for Faust code
└── FaustZPlaneProcessor.cpp  # Implementation
```

### Foleys Integration
```
source/foleys/
├── FoleysFieldEngineProcessor.h    # Processor with Foleys magic state
├── FoleysFieldEngineProcessor.cpp  # Implementation
└── fieldengine_ui.xml             # XML UI definition
```

## Setup Instructions

### 1. Install Faust

**Windows (vcpkg):**
```powershell
vcpkg install faust
```

**Manual Install:**
```bash
# Download from https://github.com/grame-cncm/faust
# Add to PATH
```

### 2. Install Foleys GUI Magic

**Git Submodule:**
```bash
cd third_party/
git submodule add https://github.com/ffAudio/foleys_gui_magic.git
git submodule update --init --recursive
```

### 3. Generate Faust Code

```bash
# Navigate to Faust directory
cd source/faust/

# Generate JUCE processor (creates C++ files)
faust2juce -cn ZPlaneMorph zplane_morph.dsp

# This creates:
# - ZPlaneMorph.h
# - ZPlaneMorph.cpp
```

### 4. CMake Integration

Add to your `CMakeLists.txt`:

```cmake
# Find Faust
find_package(faust REQUIRED)

# Add Foleys GUI Magic
add_subdirectory(third_party/foleys_gui_magic)

# Add to your plugin target
target_link_libraries(YourPlugin
    PRIVATE
        juce::juce_audio_processors
        foleys_gui_magic
        faust::faust
)

# Include directories
target_include_directories(YourPlugin PRIVATE
    source/faust/
    source/foleys/
)

# Copy XML files to build directory
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/source/foleys/fieldengine_ui.xml
    ${CMAKE_CURRENT_BINARY_DIR}/fieldengine_ui.xml
    COPYONLY
)
```

## Parameter Binding

The XML UI automatically binds to APVTS parameters by name:

```xml
<!-- This slider automatically connects to "morph" parameter -->
<Slider parameter="morph" style="alien-knob"/>
```

### Parameter Names in APVTS:
- `morph` → Morph amount (0-1)
- `cutoff` → Cutoff frequency (20-20000 Hz)
- `resonance` → Resonance (0-1)
- `drive` → Drive amount (0-24 dB)
- `output` → Output gain (-24 to +24 dB)
- `lfoRate` → LFO rate (0.02-8 Hz)
- `lfoDepth` → LFO depth (0-1)
- `envDepth` → Envelope depth (0-1)
- `dryWet` → Dry/wet mix (0-1)
- `bypass` → Bypass toggle (bool)

## Usage Examples

### 1. Replace Existing DSP with Faust

```cpp
// In your processor class
#include "FaustZPlaneProcessor.h"

class MyProcessor : public juce::AudioProcessor {
private:
    FaustZPlaneProcessor faustProcessor;

public:
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override {
        // Update Faust parameters from APVTS
        faustProcessor.setMorph(*apvts.getRawParameterValue("morph"));
        faustProcessor.setCutoff(*apvts.getRawParameterValue("cutoff"));

        // Process with Faust
        faustProcessor.processBlock(buffer, {});
    }
};
```

### 2. Use Foleys XML UI

```cpp
// In your processor
#include "FoleysFieldEngineProcessor.h"

juce::AudioProcessorEditor* createEditor() override {
    return new foleys::MagicPluginEditor(
        magicState,
        BinaryData::fieldengine_ui_xml,
        BinaryData::fieldengine_ui_xmlSize
    );
}
```

### 3. Live GUI Editing

During development, enable live editing:

```cpp
// In debug builds
#if JUCE_DEBUG
    magicState.setGuiValueTree(BinaryData::fieldengine_ui_xml);
    // Changes to XML file are reflected immediately
#endif
```

## Advantages

### Faust Benefits:
- ✅ **Auto-optimized code** (SIMD, vectorization)
- ✅ **Mathematically proven stability**
- ✅ **Cross-platform compilation**
- ✅ **Functional paradigm** (no side effects)
- ✅ **Built-in analysis tools**

### Foleys Benefits:
- ✅ **Live GUI editing** (no recompilation)
- ✅ **Automatic parameter binding**
- ✅ **Built-in visualizers** (spectrum, meters)
- ✅ **Responsive layouts**
- ✅ **Custom component support**

## Migration Strategy

### Phase 1: Faust DSP
1. Implement Z-plane morphing in Faust
2. Test against existing EMUFilter/MorphFilter
3. Benchmark performance improvements
4. Replace production DSP when validated

### Phase 2: Foleys UI
1. Recreate existing UI layout in XML
2. Add custom components (ZPlaneVisualizer, GlyphKnob)
3. Migrate parameters one section at a time
4. Maintain existing look and feel

### Phase 3: Advanced Features
1. Add real-time spectrum analysis
2. Implement preset browser in XML
3. Add automation recording/playback
4. Create modular UI components

## Troubleshooting

### Faust Issues:
- **Missing symbols**: Ensure Faust libraries are linked
- **Sample rate issues**: Call `faustProcessor->init(sampleRate)`
- **Parameter not found**: Check parameter names in generated code

### Foleys Issues:
- **XML not loading**: Check file path and binary data
- **Parameters not binding**: Verify APVTS parameter IDs match XML
- **Styling issues**: Check color format (0xAARRGGBB)

## Next Steps

1. **Test Faust compilation**: `faust2juce zplane_morph.dsp`
2. **Add to build system**: Update CMakeLists.txt
3. **Create hybrid processor**: Combine Faust DSP + Foleys UI
4. **Performance testing**: Compare against existing implementation
5. **UI refinement**: Match existing alien theme styling