cmake_minimum_required(VERSION 3.21)

project(fe_secretfx LANGUAGES CXX)

# Embed authentic banks directly into the binary
juce_add_binary_data(fe_secretfx_data
    NAMESPACE SecretFXData
    SOURCES
        "${CMAKE_SOURCE_DIR}/rich data/01_EMU_ZPlane/src/EMU_ZPlane_Vault/AUTHENTIC/shapes/audity_shapes_A_48k.json"
        "${CMAKE_SOURCE_DIR}/rich data/01_EMU_ZPlane/src/EMU_ZPlane_Vault/AUTHENTIC/shapes/audity_shapes_B_48k.json")

add_library(fe_secretfx STATIC
    src/SecretFX.cpp
)

target_include_directories(fe_secretfx PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include)

target_link_libraries(fe_secretfx PRIVATE
    juce::juce_dsp
    juce::juce_core
    fe_secretfx_data
)

target_compile_features(fe_secretfx PUBLIC cxx_std_17)

# Hide symbols by default; this stays invisible in plugin exports
set_target_properties(fe_secretfx PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES)

target_compile_definitions(fe_secretfx PRIVATE FE_SECRETFX_INTERNAL=1)
