# AI Agent Prompts for morphEngine Enhancement

## DSP Agent Prompts

### 1. Optimized EMU Z-Plane Filter Implementation
```
Generate a high-performance EMU Z-plane filter class that implements authentic pole interpolation between Vowel, Bell, and Low models. The class should:

- Use SIMD optimization for biquad processing
- Implement real-time safe parameter smoothing
- Support zero-latency mode with fractional delay compensation
- Include proper denormal protection
- Use pre-computed coefficient tables for morphing

The filter should handle stereo processing and accept morph (0-1) and resonance (0-1) parameters.
```

### 2. Lock-Free Parameter Smoothing System
```
Create a lock-free parameter smoothing system for real-time audio that:

- Uses atomic operations for thread-safe parameter updates
- Implements exponential smoothing with configurable time constants
- Supports both linear and logarithmic parameter scaling
- Prevents audio artifacts during parameter changes
- Works with JUCE AudioProcessorValueTreeState

Include smoothers for all morphEngine parameters: morph, resonance, mix, drive, hardness, brightness.
```

### 3. SIMD-Optimized Audio Processing Loop
```
Generate a SIMD-optimized processBlock implementation for morphEngine that:

- Processes stereo audio in vectorized chunks
- Minimizes cache misses with proper data alignment
- Uses SSE/AVX instructions for parallel processing
- Implements proper loop unrolling
- Maintains sample accuracy for parameter changes

The implementation should handle the complete DSP chain: drive → z-plane filter → style processing → mix.
```

## UI Agent Prompts

### 1. Hardware-Inspired Terminal LookAndFeel
```
Create a custom JUCE LookAndFeel class called "TerminalHardwareLookAndFeel" that implements:

- Monospace font rendering with anti-aliasing
- High-contrast color scheme (black/green/yellow/blue)
- ASCII-style borders and decorations
- Hardware button styling with LED indicators
- Terminal text rendering with cursor effects
- Proper DPI scaling support

Include custom drawing methods for buttons, sliders, and labels with the terminal aesthetic.
```

### 2. Real-Time ASCII Spectrum Analyzer
```
Generate a JUCE Component class for real-time ASCII spectrum display that:

- Uses lock-free ring buffer for audio data transfer
- Implements FFT analysis with proper windowing
- Renders spectrum using ASCII block characters (█▓▒░)
- Updates at 30fps with smooth decay
- Shows filter response curve overlay
- Supports variable frequency ranges

The component should integrate with morphEngine's audio processor for real-time visualization.
```

### 3. XY Morph Pad with Terminal Styling
```
Create an advanced XY pad component for morphEngine with:

- Smooth mouse/touch interaction with momentum
- Terminal-style crosshair grid
- Corner labels (VOWEL/METAL/LOW/CRUSH)
- Real-time parameter feedback
- Accessibility support for keyboard navigation
- Visual feedback for parameter automation

Include proper JUCE parameter attachment and host automation support.
```

## Glue/Refactor Agent Prompts

### 1. Complete APVTS Parameter Layout
```
Generate a complete AudioProcessorValueTreeState::ParameterLayout for morphEngine that:

- Defines all parameters with proper ranges and skew factors
- Includes parameter automation metadata
- Sets up appropriate parameter groups
- Implements proper unit conversion (dB, Hz, %)
- Includes parameter validation and clamping
- Supports preset save/load functionality

Use the morphEngine specification provided to create production-ready parameter definitions.
```

### 2. Plugin Architecture Refactoring
```
Refactor the morphEngine plugin architecture to:

- Separate DSP processing into modular components
- Implement proper dependency injection for testability
- Create clean interfaces between UI and audio processing
- Add comprehensive error handling and validation
- Implement proper resource management (RAII)
- Support multiple sample rates and buffer sizes

Focus on maintainable, extensible code that follows JUCE best practices.
```

### 3. Preset Management System
```
Create a comprehensive preset management system that:

- Loads/saves presets to XML format
- Implements factory preset initialization
- Supports user preset banks
- Includes preset morphing between states
- Handles version compatibility
- Integrates with host preset systems

Include proper error handling and validation for corrupt preset data.
```

## Advanced Enhancement Prompts

### 1. Multi-Band Processing Extension
```
Extend morphEngine to support multi-band processing with:

- Linkwitz-Riley crossover filters
- Independent Z-plane processing per band
- Visual band split indicators
- Per-band bypass controls
- Frequency-dependent style processing

Maintain the terminal UI aesthetic while adding band controls.
```

### 2. MIDI Learn Functionality
```
Implement MIDI learn system for morphEngine that:

- Maps MIDI controllers to plugin parameters
- Supports 14-bit high-resolution MIDI
- Includes MIDI parameter smoothing
- Saves MIDI mappings with presets
- Provides visual feedback for learned controls

Integrate seamlessly with the existing parameter system.
```

### 3. GPU-Accelerated Visualization
```
Create OpenGL-based visualization for morphEngine that:

- Renders real-time spectrum with smooth animations
- Shows 3D Z-plane response surface
- Implements particle effects for filter morphing
- Supports multiple color schemes
- Maintains 60fps performance
- Gracefully falls back to CPU rendering

Keep the terminal aesthetic while adding modern visual effects.
```

## Usage Instructions

1. **Setup**: Add your Gemini API key to `.env.local`
2. **Run**: `npm run dev` in the workbench directory
3. **Load Spec**: Import the `morphEngine_spec.json` file
4. **Select Agent**: Choose DSP, UI, or Glue/Refactor agent
5. **Enter Prompt**: Use one of the above prompts or create custom ones
6. **Generate**: Let AI create optimized JUCE code
7. **Integrate**: Review and integrate generated code into morphEngine

The AI agents will generate production-ready C++ code that follows JUCE best practices and real-time safety constraints.