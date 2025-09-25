cmake_minimum_required(VERSION 3.15)
project(EMU_ZPlane VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# EMU Z-Plane Library
add_library(emu_zplane STATIC
    AuthenticEMUZPlane.cpp
    EMUFilter.cpp
    ZPlaneFilter.h
    ZPlaneTables.h
    emu_zplane_api.h
)

target_include_directories(emu_zplane PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Optional: JUCE Plugin Integration Example
option(BUILD_JUCE_PLUGIN "Build JUCE VST3 plugin example" OFF)

if(BUILD_JUCE_PLUGIN)
    # Find JUCE (assumes JUCE is available)
    find_package(PkgConfig REQUIRED)

    # Add JUCE plugin target
    add_library(EMU_ZPlane_VST3 MODULE
        FieldEngineFXProcessor.cpp
        # Add your JUCE plugin sources here
    )

    target_link_libraries(EMU_ZPlane_VST3 PRIVATE
        emu_zplane
        # juce::juce_audio_processors
        # juce::juce_audio_utils
        # Add your JUCE libraries here
    )

    # VST3 properties
    set_target_properties(EMU_ZPlane_VST3 PROPERTIES
        BUNDLE TRUE
        BUNDLE_EXTENSION vst3
    )
endif()

# Installation
install(TARGETS emu_zplane
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES
    emu_zplane_api.h
    manifest.json
    README.md
    DESTINATION include
)

install(DIRECTORY
    shapes/
    banks/
    DESTINATION share/emu_zplane
)