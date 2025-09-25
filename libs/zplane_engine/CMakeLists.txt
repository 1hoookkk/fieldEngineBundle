cmake_minimum_required(VERSION 3.16)

add_library(zplane_engine
    src/DspBridge.cpp
)

target_include_directories(zplane_engine
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include
        ${CMAKE_SOURCE_DIR}/plugins/_shared
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/src
)

target_link_libraries(zplane_engine
    PUBLIC
        zplane_core
        zplane_models
)

target_compile_features(zplane_engine PUBLIC cxx_std_17)
