cmake_minimum_required(VERSION 3.16)

add_library(zplane_core
    src/ZPlaneSpectralMask.cpp
)

target_include_directories(zplane_core
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include
        ${CMAKE_SOURCE_DIR}/JUCE/modules
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/src
)

target_compile_features(zplane_core PUBLIC cxx_std_17)

