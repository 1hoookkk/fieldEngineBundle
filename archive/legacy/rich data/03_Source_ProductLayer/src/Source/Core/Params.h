#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

namespace Params
{
// Parameter IDs for the plugin
namespace ParameterIDs
{
    // Dual-Mode Engine Parameters
    const juce::String mode = "mode";                     // 0=Synth, 1=Resynth, 2=Hybrid
    const juce::String blend = "blend";                   // 0..1 (0=synth only, 1=resynth only)
    // respeed removed for live insert mode
    
    // Brush System Parameters
    const juce::String brushType = "brush_type";          // 0=Paint, 1=Quantize, 2=Erase, 3=Comb
    const juce::String brushSize = "brush_size";          // 1..128 bins
    const juce::String brushStrength = "brush_strength";  // 0..1
    const juce::String quantizeStrength = "quantize_strength"; // 0..1
    
    // Grid & Tuning Parameters
    const juce::String gridOn = "grid_on";
    const juce::String tuningId = "tuning_id";            // 0=12TET, custom N-EDO...
    
    // Z-Plane Morph Parameters
    const juce::String zPlaneMorphX = "zplane_morph_x";   // 0..1
    const juce::String zPlaneMorphY = "zplane_morph_y";   // 0..1
    
    // Z-Plane Filter Model Parameters - TEMPORARILY DISABLED
    /*
    const juce::String zplaneModelA = "zplane_model_a";   // Model A selection
    const juce::String zplaneModelB = "zplane_model_b";   // Model B selection  
    const juce::String zplaneMorph = "zplane_morph";      // A/B morph amount
    const juce::String zplaneQuality = "zplane_quality";  // Quality mode
    */
    
    // Spectral Processing Parameters
    const juce::String spectralGain = "spectral_gain";
    const juce::String spectralMix = "spectral_mix";
    const juce::String fftSize = "fft_size";
    const juce::String hopSize = "hop_size";
    
    // Z-Plane Filter Parameters  
    const juce::String filterType = "filter_type";       // 0.0=LP, 0.5=BP, 1.0=HP
    const juce::String filterCutoff = "filter_cutoff";
    const juce::String filterResonance = "filter_resonance";
    const juce::String filterMorphAmount = "filter_morph_amount";
    
    // Tube Saturation Parameters
    const juce::String saturationDrive = "saturation_drive";
    const juce::String saturationMix = "saturation_mix";
    const juce::String saturationOversampling = "saturation_oversampling";
    
    // Visual Parameters
    const juce::String colormapType = "colormap_type";   // 0=Viridis, 1=Magma, 2=Purple_Cyan
    const juce::String particleCount = "particle_count";
    const juce::String animationSpeed = "animation_speed";
    
    // Performance Parameters
    const juce::String fastPaintMode = "fast_paint_mode"; // Low-latency mode for interaction
    const juce::String gpuAcceleration = "gpu_acceleration";
    
    // Phase 2-3 Minimal UI Parameters
    const juce::String testModeActive = "test_mode_active"; // Start/Stop test toggle
    const juce::String maskDepth = "mask_depth";           // 0..1 mask intensity
    const juce::String debugOverlayEnabled = "debug_overlay_enabled"; // Debug overlay toggle
    const juce::String visualGamma = "visual_gamma";       // 0.5..1.5 display gamma (Phase 3)
    const juce::String visualGain = "visual_gain";         // 0..4 display gain (Phase 3)
    const juce::String freezeDisplay = "freeze_display";   // Freeze/scroll toggle (Phase 3)
    
    // Phase 5 Performance HUD Parameters  
    const juce::String showPerfHud = "show_perf_hud";     // Performance overlay toggle
    // Process enable (Bypass ↔ Process)
    const juce::String processEnabled = "process_enabled"; // 0/1 toggle for two-mode router
    
    // Master Parameters
    const juce::String gain = "gain";                     // -24..+24 dB master gain
    
    // Phase 4 Experiment Parameters
    const juce::String useTestFeeder = "use_test_feeder";         // Rollback to old system
    const juce::String keyFilterEnabled = "key_filter_enabled";   // Enable key filter
    const juce::String scaleType = "scale_type";                  // 0=Chromatic, 1=Major, 2=Minor  
    const juce::String rootNote = "root_note";                    // 0-11 pitch classes
    const juce::String oscGain = "osc_gain";                      // 0-1 oscillator gain
    
    // A/B Testing Parameters
    const juce::String useModernPaint = "use_modern_paint";       // A/B switch: ModernPaint vs Phase4Synth
    
    // Psychoacoustic Masking Parameters
    const juce::String maskingStrictness = "masking_strictness";   // 0.5-2.0 JND multiplier
    const juce::String jndMarginDB = "jnd_margin_db";             // 0-6dB safety margin
    const juce::String showMaskingOverlay = "show_masking_overlay"; // bool heat map toggle
    const juce::String decayTauPreset = "decay_tau_preset";        // 0=120ms,1=200ms,2=300ms
}

// Parameter ranges and defaults
namespace ParameterRanges
{
    // Dual-Mode Engine
    const juce::NormalisableRange<float> modeRange(0.0f, 2.0f, 1.0f);
    const float modeDefault = 0.0f; // Synth
    
    const juce::NormalisableRange<float> blendRange(0.0f, 1.0f, 0.01f);
    const float blendDefault = 0.0f; // Full synth
    
    // respeed range removed
    
    // Brush System
    const juce::NormalisableRange<float> brushTypeRange(0.0f, 3.0f, 1.0f);
    const float brushTypeDefault = 0.0f; // Paint
    
    const juce::NormalisableRange<float> brushSizeRange(1.0f, 128.0f, 1.0f);
    const float brushSizeDefault = 16.0f;
    
    const juce::NormalisableRange<float> brushStrengthRange(0.0f, 1.0f, 0.01f);
    const float brushStrengthDefault = 0.7f;
    
    const juce::NormalisableRange<float> quantizeStrengthRange(0.0f, 1.0f, 0.01f);
    const float quantizeStrengthDefault = 0.5f;
    
    // Grid & Tuning
    const juce::NormalisableRange<float> tuningIdRange(0.0f, 10.0f, 1.0f);
    const float tuningIdDefault = 0.0f; // 12TET
    
    // Z-Plane Morph
    const juce::NormalisableRange<float> zPlaneMorphRange(0.0f, 1.0f, 0.01f);
    const float zPlaneMorphXDefault = 0.5f;
    const float zPlaneMorphYDefault = 0.5f;
    
    // Spectral Processing
    const juce::NormalisableRange<float> spectralGainRange(0.0f, 2.0f, 0.01f);
    const float spectralGainDefault = 1.0f;
    
    const juce::NormalisableRange<float> spectralMixRange(0.0f, 1.0f, 0.01f);
    const float spectralMixDefault = 1.0f;
    
    const juce::NormalisableRange<float> fftSizeRange(256.0f, 2048.0f, 1.0f);
    const float fftSizeDefault = 512.0f;
    
    // Z-Plane Filter
    const juce::NormalisableRange<float> filterTypeRange(0.0f, 1.0f, 0.01f);
    const float filterTypeDefault = 0.0f; // Lowpass
    
    const juce::NormalisableRange<float> filterCutoffRange(20.0f, 20000.0f, 1.0f);
    const float filterCutoffDefault = 1000.0f;
    
    const juce::NormalisableRange<float> filterResonanceRange(0.1f, 10.0f, 0.01f);
    const float filterResonanceDefault = 0.7f;
    
    // Tube Saturation
    const juce::NormalisableRange<float> saturationDriveRange(0.0f, 10.0f, 0.01f);
    const float saturationDriveDefault = 1.0f;
    
    const juce::NormalisableRange<float> saturationMixRange(0.0f, 1.0f, 0.01f);
    const float saturationMixDefault = 0.3f;
    
    // Visual
    const juce::NormalisableRange<float> colormapTypeRange(0.0f, 2.0f, 1.0f);
    const float colormapTypeDefault = 2.0f; // Purple_Cyan (nebula default)
    
    const juce::NormalisableRange<float> particleCountRange(100.0f, 5000.0f, 10.0f);
    const float particleCountDefault = 1000.0f;
    
    // Phase 2-3 Minimal UI
    const float maskDepthDefault = 0.7f; // Default mask intensity
    const juce::NormalisableRange<float> maskDepthRange(0.0f, 1.0f, 0.01f);
    
    const juce::NormalisableRange<float> visualGammaRange(0.5f, 1.5f, 0.01f);
    const float visualGammaDefault = 1.0f;
    
    const juce::NormalisableRange<float> visualGainRange(0.0f, 4.0f, 0.01f);
    const float visualGainDefault = 1.0f;
    
    // Phase 5 Performance HUD
    const juce::NormalisableRange<float> showPerfHudRange(0.0f, 1.0f, 1.0f);
    const float showPerfHudDefault = 0.0f; // Off by default
    
    // Master
    const juce::NormalisableRange<float> gainRange(-24.0f, 24.0f, 0.01f);
    const float gainDefault = 0.0f;
    
    // Phase 4 Experiment Parameters
    const float useTestFeederDefault = 0.0f;             // Use Phase4Synth by default for paint processing
    const float keyFilterEnabledDefault = 1.0f;          // Key filter enabled (true)
    const juce::NormalisableRange<float> scaleTypeRange(0.0f, 2.0f, 1.0f);
    const float scaleTypeDefault = 1.0f;                 // Major scale
    const juce::NormalisableRange<float> rootNoteRange(0.0f, 11.0f, 1.0f);
    const float rootNoteDefault = 0.0f;                  // C
    const juce::NormalisableRange<float> oscGainRange(0.0f, 1.0f, 0.01f);
    const float oscGainDefault = 0.2f;                   // Conservative gain
    
    // A/B Testing Parameters
    const float useModernPaintDefault = 1.0f;            // Default to ModernPaint (true)
    
    // Psychoacoustic Masking Parameters  
    const juce::NormalisableRange<float> maskingStrictnessRange(0.5f, 2.0f, 0.01f);
    const float maskingStrictnessDefault = 1.0f;
    
    const juce::NormalisableRange<float> jndMarginDBRange(0.0f, 6.0f, 0.1f);
    const float jndMarginDBDefault = 3.0f;
    
    const float showMaskingOverlayDefault = false;

    // UI-facing Morph Filter (always-on stage; parameters are internal-safe) - TEMPORARILY DISABLED
    /*
    static constexpr const char* zplaneModelA   = "zplane_model_a";
    static constexpr const char* zplaneModelB   = "zplane_model_b";
    static constexpr const char* zplaneMorph    = "zplane_morph_amount";
    static constexpr const char* zplaneQuality  = "zplane_quality";
    */
}

// Create parameter layout for ValueTreeState
inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameters;
    
    // Dual-Mode Engine Parameters
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::mode, "Mode",
        ParameterRanges::modeRange, ParameterRanges::modeDefault));
    
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::blend, "Blend",
        ParameterRanges::blendRange, ParameterRanges::blendDefault));
    
    // no respeed parameter
    
    // Brush System Parameters
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::brushType, "Brush Type",
        ParameterRanges::brushTypeRange, ParameterRanges::brushTypeDefault));
    
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::brushSize, "Brush Size",
        ParameterRanges::brushSizeRange, ParameterRanges::brushSizeDefault));
    
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::brushStrength, "Brush Strength",
        ParameterRanges::brushStrengthRange, ParameterRanges::brushStrengthDefault));
    
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::quantizeStrength, "Quantize Strength",
        ParameterRanges::quantizeStrengthRange, ParameterRanges::quantizeStrengthDefault));
    
    // Grid & Tuning Parameters
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(
        ParameterIDs::gridOn, "Grid On", false));
    
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::tuningId, "Tuning",
        ParameterRanges::tuningIdRange, ParameterRanges::tuningIdDefault));
    
    // Z-Plane Morph Parameters
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::zPlaneMorphX, "Z-Plane Morph X",
        ParameterRanges::zPlaneMorphRange, ParameterRanges::zPlaneMorphXDefault));
    
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::zPlaneMorphY, "Z-Plane Morph Y",
        ParameterRanges::zPlaneMorphRange, ParameterRanges::zPlaneMorphYDefault));
    
    // Spectral Processing Parameters
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::spectralGain, "Spectral Gain",
        ParameterRanges::spectralGainRange, ParameterRanges::spectralGainDefault));
    
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::spectralMix, "Spectral Mix",
        ParameterRanges::spectralMixRange, ParameterRanges::spectralMixDefault));
    
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::fftSize, "FFT Size",
        ParameterRanges::fftSizeRange, ParameterRanges::fftSizeDefault));
    
    // Z-Plane Filter Parameters
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::filterType, "Filter Type",
        ParameterRanges::filterTypeRange, ParameterRanges::filterTypeDefault));
    
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::filterCutoff, "Filter Cutoff",
        ParameterRanges::filterCutoffRange, ParameterRanges::filterCutoffDefault));
    
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::filterResonance, "Filter Resonance",
        ParameterRanges::filterResonanceRange, ParameterRanges::filterResonanceDefault));
    
    // Tube Saturation Parameters
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::saturationDrive, "Saturation Drive",
        ParameterRanges::saturationDriveRange, ParameterRanges::saturationDriveDefault));
    
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::saturationMix, "Saturation Mix",
        ParameterRanges::saturationMixRange, ParameterRanges::saturationMixDefault));
    
    // Visual Parameters
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::colormapType, "Colormap Type",
        ParameterRanges::colormapTypeRange, ParameterRanges::colormapTypeDefault));
    
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::particleCount, "Particle Count",
        ParameterRanges::particleCountRange, ParameterRanges::particleCountDefault));

    // Z-Plane Morph Filter parameters have been removed to fix compilation errors
    
    // Performance Parameters
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(
        ParameterIDs::fastPaintMode, "Fast Paint Mode", false));
    
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(
        ParameterIDs::gpuAcceleration, "GPU Acceleration", true));
    
    // Phase 2-3 Minimal UI Parameters
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(
        ParameterIDs::testModeActive, "Test Mode Active", false));
    
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::maskDepth, "Mask Depth",
        ParameterRanges::maskDepthRange, ParameterRanges::maskDepthDefault));
    
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(
        ParameterIDs::debugOverlayEnabled, "Debug Overlay", true));
    
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::visualGamma, "Visual Gamma",
        ParameterRanges::visualGammaRange, ParameterRanges::visualGammaDefault));
    
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::visualGain, "Visual Gain",
        ParameterRanges::visualGainRange, ParameterRanges::visualGainDefault));
    
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(
        ParameterIDs::freezeDisplay, "Freeze Display", false));
    
    // Phase 5 Performance HUD
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::showPerfHud, "Show Performance HUD",
        ParameterRanges::showPerfHudRange, ParameterRanges::showPerfHudDefault));
    
    // Two-mode router: Bypass (Silent) ↔ Process (Phase4/SpectralResynthesis)
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(
        ParameterIDs::processEnabled, "Process Enabled", true));
    
    // Master Gain
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::gain, "Master Gain",
        ParameterRanges::gainRange, ParameterRanges::gainDefault));
    
    // Phase 4 Experiment Parameters
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(
        ParameterIDs::useTestFeeder, "Use Test Feeder",
        ParameterRanges::useTestFeederDefault));
        
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(
        ParameterIDs::keyFilterEnabled, "Key Filter Enabled", 
        ParameterRanges::keyFilterEnabledDefault));
        
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::scaleType, "Scale Type",
        ParameterRanges::scaleTypeRange, ParameterRanges::scaleTypeDefault));
        
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::rootNote, "Root Note",
        ParameterRanges::rootNoteRange, ParameterRanges::rootNoteDefault));
        
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::oscGain, "Oscillator Gain",
        ParameterRanges::oscGainRange, ParameterRanges::oscGainDefault));
    
    // A/B Testing Parameters
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(
        ParameterIDs::useModernPaint, "Use Modern Paint",
        ParameterRanges::useModernPaintDefault));
    
    // Psychoacoustic Masking Parameters
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::maskingStrictness, "Masking Strictness",
        ParameterRanges::maskingStrictnessRange, ParameterRanges::maskingStrictnessDefault));
        
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParameterIDs::jndMarginDB, "JND Margin (dB)",
        ParameterRanges::jndMarginDBRange, ParameterRanges::jndMarginDBDefault));
        
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(
        ParameterIDs::showMaskingOverlay, "Show Masking Overlay",
        ParameterRanges::showMaskingOverlayDefault));

    // Decay Tau preset (choice)
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParameterIDs::decayTauPreset,
        "Decay Tau",
        juce::StringArray{"Fast (120ms)", "Medium (200ms)", "Slow (300ms)"},
        1));
    
    return { parameters.begin(), parameters.end() };
}

} // namespace Params