
cmake_minimum_required(VERSION 3.19)
project(HardTunePlugin VERSION 0.1.0)

# Expect JUCE from environment: -DJUCE_DIR=/path/to/JUCE
if (NOT JUCE_DIR)
  message(FATAL_ERROR "Please configure JUCE by setting -DJUCE_DIR=/path/to/JUCE")
endif()
add_subdirectory(${JUCE_DIR} JUCE)

# Add our internal DSP library (header+cpps from libs/pitchengine_dsp)
set(PITCHENGINE_DSP_DIR ${CMAKE_CURRENT_LIST_DIR}/../../libs/pitchengine_dsp)
file(GLOB PITCHENGINE_DSP_SOURCES
     ${PITCHENGINE_DSP_DIR}/src/*.cpp)
add_library(pitchengine_dsp STATIC ${PITCHENGINE_DSP_SOURCES})
target_include_directories(pitchengine_dsp PUBLIC ${PITCHENGINE_DSP_DIR}/include)

# Plugin target
juce_add_plugin(HardTune
    COMPANY_NAME "YourCompany"
    BUNDLE_ID com.yourcompany.hardtune
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Juce
    PLUGIN_CODE Htun
    FORMATS VST3 AU
    PRODUCT_NAME "HardTune")

target_sources(HardTune PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/Source/PluginProcessor.cpp
    ${CMAKE_CURRENT_LIST_DIR}/Source/PluginProcessor.h
    ${CMAKE_CURRENT_LIST_DIR}/Source/PluginEditor.cpp
    ${CMAKE_CURRENT_LIST_DIR}/Source/PluginEditor.h
)

target_compile_features(HardTune PRIVATE cxx_std_17)

target_link_libraries(HardTune
    PRIVATE
        pitchengine_dsp
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_core
)

if(APPLE AND NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "10.14" CACHE STRING "Minimum OS X deployment version")
endif()
