# SpectralCanvas VST Development Status

## Project Overview
SpectralCanvas is a VST plugin inspired by MetaSynth and Composer Desktop Pro - the legendary tools behind Aphex Twin's spectral work. We're building a modern real-time incarnation focused on beatmaker workflow with image-to-sound synthesis capabilities.

## Current Architecture: 3-Room Simplified Design

After collaboration with Gemini and user feedback, we simplified from 6 rooms to 3 essential rooms:

### 1. IMAGE SYNTH (Core Room)
- **Purpose**: Primary image-to-sound synthesis with integrated filters and effects
- **MetaSynth Mapping**: X=Time, Y=Frequency, Color=Pan (Red=Left, Green=Right), Brightness=Amplitude
- **Features**: Core synthesis + basic filters + essential effects all in one interface
- **Status**: Architecture completed, implementation pending

### 2. SPECTRUM (Spectral Manipulation)
- **Purpose**: Simplified spectral manipulation and visualization
- **Features**: Real-time spectral analysis and editing tools
- **Status**: Architecture completed, implementation pending

### 3. PROCESS (Essential Effects)
- **Purpose**: 2-3 core spectral processing effects (freeze, smear, stretch)
- **Features**: Essential CDP-inspired spectral transformations
- **Status**: Architecture completed, implementation pending

## Technical Implementation Status

### ✅ Completed Components

#### Room Management System
- **Files**: `Source/GUI/RoomManager.h`, `Source/GUI/RoomManager.cpp`
- **Features**: 
  - 3-room tabbed interface with proper switching
  - SpectralRoomComponent base class for room implementations
  - Integration with existing JUCE architecture
  - Room activation/deactivation lifecycle management

#### UI Foundation
- **Files**: `Source/GUI/ArtefactLookAndFeel.cpp`
- **Features**: Amateur/hobbyist aesthetic (Windows 95 style)
- **Colors**: Classic dialog grays, basic bevels, functional design
- **Status**: Matches desired "dated but functional" aesthetic

#### Existing Plugin Architecture
- **Files**: `Source/GUI/PluginEditor.h/.cpp`
- **Features**: Current 3-panel layout (ForgePanel, RetroCanvas, PaintControlPanel)
- **Integration**: Foundation ready for room manager integration

### 🔄 In Progress

#### JUCE Plugin Scaffold with 3-Room Architecture
- Room manager system implemented
- Integration with existing PluginEditor pending
- Tab switching and room visibility management ready

### ⏳ Pending Implementation

#### High Priority
1. **Dynamic Oscillator Pool Manager** (1024+ oscillators with garbage collection)
2. **Enhanced Image Synth Room** (core + basic filters + essential effects)
3. **Simplified Spectrum Synth Room** with intuitive spectral manipulation

#### Medium Priority
4. **Essential Spectral Processes** integrated into Image Synth room
5. **Clean 3-Room Tabbed Interface** focused on beatmaker workflow
6. **OpenCV Integration** for basic image processing
7. **Real-time Synthesis Engine** with sub-10ms latency

#### Low Priority
8. **Performance Profiling Framework**
9. **Beatmaker Workflow Testing Framework**

## Key Technical Requirements

### Real-Time Performance
- **Target Latency**: Sub-10ms for real-time performance
- **Oscillator Pool**: 1024+ oscillators with efficient garbage collection
- **Audio Thread Safety**: Non-blocking UI updates and parameter changes

### MetaSynth Compatibility
- **Image Mapping**: X=Time, Y=Frequency, Color=Stereo Pan, Brightness=Amplitude
- **Real-Time Processing**: Modern incarnation of offline MetaSynth workflow
- **Beatmaker Focus**: Streamlined for quick DAW integration and loop creation

### UI Philosophy
- **Aesthetic**: "Dated but extremely functional and satisfying"
- **Inspiration**: MetaSynth, CDP, Cool Edit Pro visual language
- **Workflow**: Beatmaker-centric with minimal room switching
- **Architecture**: 3 rooms maximum for focused workflow

## Development Notes

### Architecture Decisions
- **Simplified from 6 to 3 rooms** based on beatmaker workflow analysis
- **Integrated approach** - filters/effects within core rooms rather than separate
- **Tab-based navigation** for clean room switching
- **Existing component reuse** where possible to maintain stability

### Collaboration Approach
- **Gemini Integration**: Used for architecture decisions and brainstorming
- **User Feedback Loop**: Continuous refinement based on user vision
- **MetaSynth Study**: Deep analysis of original tool documentation

## Next Steps

1. **Integrate RoomManager** with existing PluginEditor architecture
2. **Implement ImageSynthRoom** as first functional room
3. **Add oscillator pool management** for real-time synthesis
4. **Create basic image-to-sound mapping** functionality
5. **Test beatmaker workflow** with simplified 3-room interface

---

*Last Updated: August 2, 2025*
*Status: Architecture Complete, Core Implementation Beginning*