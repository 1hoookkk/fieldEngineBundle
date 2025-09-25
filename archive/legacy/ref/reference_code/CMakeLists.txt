cmake_minimum_required(VERSION 3.16)
project(fieldEngine)

add_library(fieldEngine STATIC
  Source/Core/fieldEngine.cpp
  Source/MusicFX/EnvFollower.h
  Source/MusicFX/OnsetGate.h
  Source/MusicFX/ResonatorVoice.h
  Source/MusicFX/Chordifier.h
  Source/preset/A2KRuntime.cpp
  preset_loader_json.cpp
)
target_include_directories(fieldEngine PUBLIC Source)
target_include_directories(fieldEngine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty)
target_compile_features(fieldEngine PUBLIC cxx_std_20)

# Offline renderer tool for EMU bank validation
add_executable(fieldEngineRender
    tools/fieldEngineRender.cpp
)
target_link_libraries(fieldEngineRender PRIVATE fieldEngine)
target_compile_features(fieldEngineRender PRIVATE cxx_std_20)
