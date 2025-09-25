cmake_minimum_required(VERSION 3.20)
project(AutotuneOfflineTester)

# JUCE is at the repo root
add_subdirectory(${CMAKE_SOURCE_DIR}/../../JUCE ${CMAKE_BINARY_DIR}/juce_build)

add_executable(autotune_offline main.cpp)

target_compile_features(autotune_offline PRIVATE cxx_std_17)
target_compile_definitions(autotune_offline PRIVATE
    JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VDSP_FRAMEWORK=0
    JUCE_ALSA=0
    JUCE_JACK=0
    JUCE_BELA=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
)

target_include_directories(autotune_offline PRIVATE
    ${CMAKE_SOURCE_DIR}/../../pitchEngine/source
    ${CMAKE_SOURCE_DIR}/../../pitchEngine/source/dsp
    ${CMAKE_SOURCE_DIR}/../../pitchEngine/source/util
)

target_link_libraries(autotune_offline PRIVATE
    juce::juce_core
    juce::juce_audio_basics
    juce::juce_audio_formats
    juce::juce_dsp
    juce::juce_events
)

# On Windows, use static runtime if needed
if(MSVC)
    set_property(TARGET autotune_offline PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()