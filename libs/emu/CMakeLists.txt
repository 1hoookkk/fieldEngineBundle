# Header-only EMU engine library with authentic data and QA tools
cmake_minimum_required(VERSION 3.16)

add_library(emu_core INTERFACE)

# Core API headers
target_sources(emu_core INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/api/IZPlaneEngine.h
    ${CMAKE_CURRENT_LIST_DIR}/api/IShapeBank.h
    ${CMAKE_CURRENT_LIST_DIR}/api/StaticShapeBank.h
)

# Core processing headers
target_sources(emu_core INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/core/ZPoleMath.h
    ${CMAKE_CURRENT_LIST_DIR}/core/BiquadCascade.h
    ${CMAKE_CURRENT_LIST_DIR}/core/NonlinearStage.h
    ${CMAKE_CURRENT_LIST_DIR}/core/ZPlaneLogFreqLUT.h
    ${CMAKE_CURRENT_LIST_DIR}/core/ZPlaneCoefficientBank.h
)

# Engine implementations
target_sources(emu_core INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/engines/AuthenticEMUEngine.h
)

# Wrappers and utilities
target_sources(emu_core INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/wrappers/OversampledEngine.h
)

# Adapter system for authentic data
target_sources(emu_core INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/adapter/ZPlaneAdapter_FromAuthentic.h
)

# QA and testing tools
target_sources(emu_core INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/qa/ZPlane_STFTHarness.h
)

# Spectral processing tools
target_sources(emu_core INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/spectral/ZPlaneSpectralMask.h
)

# Expose headers
target_include_directories(emu_core INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/api
    ${CMAKE_CURRENT_LIST_DIR}/core
    ${CMAKE_CURRENT_LIST_DIR}/engines
    ${CMAKE_CURRENT_LIST_DIR}/wrappers
    ${CMAKE_CURRENT_LIST_DIR}/adapter
    ${CMAKE_CURRENT_LIST_DIR}/qa
    ${CMAKE_CURRENT_LIST_DIR}/spectral
    ${CMAKE_CURRENT_LIST_DIR}/shapes
)

# Link JUCE DSP if available when this subdir is added after JUCE
if (TARGET juce_dsp)
    target_link_libraries(emu_core INTERFACE juce_dsp)
endif()

# Link JUCE core if available (for JSON loading in adapter)
if (TARGET juce_core)
    target_link_libraries(emu_core INTERFACE juce_core)
endif()
