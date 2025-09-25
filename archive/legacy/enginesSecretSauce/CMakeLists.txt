cmake_minimum_required(VERSION 3.21)
project(EnginesSecretSauce VERSION 1.0.0 LANGUAGES CXX)

# Use local JUCE installation
add_subdirectory(../JUCE ${CMAKE_CURRENT_BINARY_DIR}/JUCE)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_definitions(JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_REPORT_APP_USAGE=0)

juce_add_plugin(EnginesSecretSauce
    COMPANY_NAME        "Engines"
    BUNDLE_ID           com.engines.secretsauce
    IS_SYNTH            FALSE
    NEEDS_MIDI_INPUT    FALSE
    NEEDS_MIDI_OUTPUT   FALSE
    IS_MIDI_EFFECT      FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Engs
    PLUGIN_CODE         SecS
    FORMATS             VST3 AU AAX
    PRODUCT_NAME        "Engines SecretSauce"
)

# Sources
target_sources(EnginesSecretSauce PRIVATE
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/PluginEditor.h
    Source/PluginEditor.cpp
    Source/SecretSauceEngine.h
    Source/SecretSauceEngine.cpp
)

# Headers
target_include_directories(EnginesSecretSauce PRIVATE Source)

# Link JUCE modules
target_link_libraries(EnginesSecretSauce PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_core
    juce::juce_data_structures
    juce::juce_graphics
    juce::juce_audio_basics
    juce::juce_audio_processors
    juce::juce_gui_basics
    juce::juce_gui_extra
)

# Compile definitions
target_compile_definitions(EnginesSecretSauce PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

# Warnings / LTO (tune per toolchain)
if(MSVC)
  target_compile_options(EnginesSecretSauce PRIVATE /W4 /permissive-)
else()
  target_compile_options(EnginesSecretSauce PRIVATE -Wall -Wextra -Wpedantic)
endif()