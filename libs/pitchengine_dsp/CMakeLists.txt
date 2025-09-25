add_library(pitchengine_dsp STATIC
    src/AuthenticEMUZPlane.cpp
    src/Analyzer.cpp
    src/PitchTracker.cpp
    src/Shifter.cpp
    src/Snapper.cpp
    src/ZPlaneStyle.cpp
)

target_include_directories(pitchengine_dsp
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include
        ${CMAKE_SOURCE_DIR}/JUCE/modules
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/src
)

target_compile_features(pitchengine_dsp PUBLIC cxx_std_17)

target_compile_definitions(pitchengine_dsp PUBLIC
    JUCE_MODULE_AVAILABLE_juce_dsp=1
    JUCE_MODULE_AVAILABLE_juce_audio_basics=1
    JUCE_MODULE_AVAILABLE_juce_core=1
    JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
)

target_link_libraries(pitchengine_dsp
    PUBLIC
        zplane_core
        zplane_models
        zplane_engine
)
