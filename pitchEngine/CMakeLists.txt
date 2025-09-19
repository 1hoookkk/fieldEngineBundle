cmake_minimum_required(VERSION 3.21)
project(pitchEngine VERSION 0.9.0 LANGUAGES CXX)

# Use local JUCE installation
add_subdirectory(../JUCE ${CMAKE_CURRENT_BINARY_DIR}/JUCE)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_definitions(JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_REPORT_APP_USAGE=0)

# Resources: embed LUT JSON and icons
juce_add_binary_data(pitchEngine_Resources SOURCES
    resources/zlut/pitchEngine_zLUT_ref48k.json
    resources/icons/appIcon.png
)

juce_add_plugin(pitchEnginePro
    COMPANY_NAME        "Your Company"
    BUNDLE_ID           com.yourcompany.pitchengine
    IS_SYNTH            FALSE
    NEEDS_MIDI_INPUT    FALSE
    NEEDS_MIDI_OUTPUT   FALSE
    IS_MIDI_EFFECT      FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    PLUGIN_MANUFACTURER_CODE YCmp
    PLUGIN_CODE         pEng
    FORMATS             AU VST3 AAX
    PRODUCT_NAME        "pitchEngine Pro"
)

# Sources
target_sources(pitchEnginePro PRIVATE
    source/PluginProcessor.cpp
    source/PluginEditor.cpp
    source/dsp/ZPlaneStyle.cpp
    source/dsp/PitchTracker.cpp
    source/dsp/Snapper.cpp
    source/dsp/Shifter.cpp
    source/dsp/Analyzer.cpp
)

# Headers
target_include_directories(pitchEnginePro PRIVATE source)

# Link JUCE modules
target_link_libraries(pitchEnginePro PRIVATE
    pitchEngine_Resources
    juce::juce_gui_extra
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_core
    juce::juce_data_structures
    juce::juce_graphics
    juce::juce_audio_basics
    juce::juce_audio_processors
    juce::juce_audio_devices
)

# Warnings / LTO (tune per toolchain)
if(MSVC)
  target_compile_options(pitchEnginePro PRIVATE /W4 /permissive-)
else()
  target_compile_options(pitchEnginePro PRIVATE -Wall -Wextra -Wpedantic)
endif()