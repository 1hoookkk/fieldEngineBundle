# enginesSecretSauce CLAUDE.md

## Purpose
Specialized EMU Z-plane filtering service providing reference implementation for authentic vintage tone shaping.

## Narrative Summary
The enginesSecretSauce service serves as the foundational EMU Z-plane filtering implementation that other services in the fieldEngineBundle ecosystem reference. While not directly modified during the h-enhance-zplane-morphing task, this service provides the baseline AuthenticEMUZPlane implementation that influenced the surgical oversampling architecture implemented in pitchEngine. The service maintains the core EMU reverse-engineering research and serves as a testing ground for new Z-plane filtering techniques.

## Key Files
- `Source/PluginProcessor.h` - Basic plugin processor structure
- `Source/PluginProcessor.cpp` - Core audio processing implementation
- `Source/AuthenticEMUZPlane.h` - Reference EMU Z-plane filter implementation
- `Source/SecretSauceEngine.h` - Custom DSP engine interface
- `Source/SecretSauceEngine.cpp` - Engine implementation
- `CMakeLists.txt` - JUCE VST3 build configuration

## Integration Points
### Provides to pitchEngine
- Reference AuthenticEMUZPlane architecture that influenced surgical oversampling design
- EMU reverse-engineering methodologies and pole interpolation patterns
- Baseline Z-plane filtering concepts used in ZPlaneHelpers.h

### Consumes
- JUCE Audio Processors: Standard plugin framework
- JUCE DSP: Core digital signal processing modules
- Local JUCE installation: Shared build dependency

## Configuration
### Build Requirements
- JUCE framework: Audio processor and DSP modules
- CMake 3.21+: Build system
- C++17: Language standard
- Plugin formats: VST3, AU, AAX

### Plugin Identity
- Company: Engines
- Bundle ID: com.engines.secretsauce
- Plugin Code: SecS
- Manufacturer: Engs

## Key Patterns
### Reference Implementation Pattern
Serves as the canonical implementation of EMU Z-plane filtering that other services adapt and extend, providing stable baseline for architectural decisions.

### Research Service Pattern  
Functions as isolated testing environment for EMU reverse-engineering research without affecting production services like pitchEngine.

## Related Documentation
- `pitchEngine/CLAUDE.md` - Production service using EMU concepts from this reference
- `pitchEngine/source/dsp/AuthenticEMUZPlane.h` - Extended implementation based on this service
- Root `CMakeLists.txt` - Build system integration