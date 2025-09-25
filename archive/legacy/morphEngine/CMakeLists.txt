cmake_minimum_required(VERSION 3.21)
project(EMUMorphPro VERSION 1.0.0)

# Universal Binary on macOS
set(CMAKE_OSX_ARCHITECTURES arm64 x86_64)

# Add JUCE
add_subdirectory(JUCE)

# Create our plugin
juce_add_plugin(EMUMorphPro
    COMPANY_NAME "YourCompany"
    PLUGIN_MANUFACTURER_CODE YCmp
    PLUGIN_CODE EMUp
    FORMATS VST3 AU
    PRODUCT_NAME "EMU Morph Pro"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_CATEGORIES "Fx" "Filter"
    AU_MAIN_TYPE "kAudioUnitType_Effect"
)

# Plugin sources
target_sources(EMUMorphPro PRIVATE
    source/PluginProcessor.cpp
    source/PluginEditor.cpp
    source/ui/XtremeLeadLookAndFeel.cpp
    libs/emu_engine/src/AuthenticEMUZPlane.cpp
)

# Include directories
target_include_directories(EMUMorphPro PRIVATE
    libs/emu_engine/include
)

# Link libraries
target_link_libraries(EMUMorphPro PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# Plugin settings
target_compile_definitions(EMUMorphPro PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

# C++17 minimum
target_compile_features(EMUMorphPro PRIVATE cxx_std_17)