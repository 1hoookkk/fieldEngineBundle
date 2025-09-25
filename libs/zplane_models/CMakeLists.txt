cmake_minimum_required(VERSION 3.16)

add_library(zplane_models
    src/Zmf1.cpp
    src/Zmf1Loader.cpp
    src/PackLoader.cpp
    src/PackUtil.cpp
)

target_include_directories(zplane_models
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/src
        ${CMAKE_SOURCE_DIR}/plugins/_shared
        ${CMAKE_SOURCE_DIR}/libs/zplane_engine/include
        ${CMAKE_SOURCE_DIR}/libs/zplane_core/include
)

target_compile_features(zplane_models PUBLIC cxx_std_17)
