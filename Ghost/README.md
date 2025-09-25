# VST3 Plugin Collection

## Available Plugins

### 1. pitchEngine Pro.vst3
- **Type**: Audio Effect (Pitch Correction)
- **Features**: Real-time pitch correction with Style morphing
- **Status**: ✅ READY (v1.1 complete)
- **Key**: Hard-tune with mysterious "Style" control

### 2. FieldEngineFX.vst3
- **Type**: Audio Effect
- **Features**: Z-plane morphing filter
- **Status**: ⚠️ Built with warnings
- **Key**: EMU-style morphing filters

### 3. FieldEngineSynth.vst3
- **Type**: Instrument (Synthesizer)
- **Features**: Synthesis engine with morphing
- **Status**: ⚠️ Built with warnings
- **Key**: Full synthesizer plugin

## Installation

Copy to your VST3 folder:
- **Windows**: `C:\Program Files\Common Files\VST3\`
- **macOS**: `~/Library/Audio/Plug-Ins/VST3/`

## Testing Order

1. Test **pitchEngine Pro** first (most stable)
2. Test **FieldEngineFX** second
3. Test **FieldEngineSynth** last

## Build Locations

- pitchEngine: `pitchEngine/build/pitchEnginePro_artefacts/Release/VST3/`
- FieldEngine: `build/FieldEngineFX_artefacts/Release/VST3/`
- FieldEngine: `build/FieldEngineSynth_artefacts/Release/VST3/`