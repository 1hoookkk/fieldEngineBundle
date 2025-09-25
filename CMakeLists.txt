cmake_minimum_required(VERSION 3.25)

# Fix CMP0177 warning
if (POLICY CMP0177)
    cmake_policy(SET CMP0177 NEW)
endif()

# This tells cmake we have goodies in the /cmake folder
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include (PamplejuceVersion)

# Modern concise way to add dependencies to your project
include (CPM)

# Configures universal binaries and decides which version of macOS to support
include(PamplejuceMacOS)

# Couple tweaks that IMO should be JUCE defaults
include(JUCEDefaults)

# Change me!
# This is the internal name of the project and the name of JUCE's shared code target
# Note: This cannot have spaces (it may be 2024, but you can't have it all!)
# Worry not, JUCE's PRODUCT_NAME can have spaces (and is what DAWs display)
set(PROJECT_NAME "FieldEngine")

# Worry not, JUCE's PRODUCT_NAME can have spaces (and is what DAWs will display)
# You can also just have it be the same thing as PROJECT_NAME
# You may want to append the major version on the end of this (and PROJECT_NAME) ala:
#   set(PROJECT_NAME "MyPlugin_v${MAJOR_VERSION}")
# Doing so enables major versions to show up in IDEs and DAWs as separate plugins
# allowing you to change parameters and behavior without breaking existing user projects
set(PRODUCT_NAME "FieldEngine")

# Change me! Used for the MacOS bundle name and Installers
set(COMPANY_NAME "FieldEngine Company")

# Change me! Used for the MacOS bundle identifier (and signing)
set(BUNDLE_ID "com.fieldengine.fieldengine")

# Change me! Set the plugin formats you want built
# Valid choices: AAX Unity VST VST3 AU AUv3 Standalone
set(FORMATS Standalone VST3)

# Force JUCE to work with sccache
# instruct MSVC to embed debug info (/Z7) instead of emitting a .pdb
set(CMAKE_POLICY_DEFAULT_CMP0141        NEW      CACHE STRING "" FORCE)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded CACHE STRING "" FORCE)

# For simplicity, the name of the CMake project is also the name of the target
project(${PROJECT_NAME} VERSION ${CURRENT_VERSION})

# JUCE is setup as a submodule in the /JUCE folder
# Locally, you must run `git submodule update --init --recursive` once
# and later `git submodule update --remote --merge` to keep it up to date
# On Github Actions, this is done as a part of actions/checkout
add_subdirectory(JUCE)

# Add CLAP format
add_subdirectory(modules/clap-juce-extensions EXCLUDE_FROM_ALL)

# Add any other modules you want modules here, before the juce_add_plugin call
# juce_add_module(modules/my_module)

# This adds the melatonin inspector module
add_subdirectory (modules/melatonin_inspector)

# Add modular EMU engine library
add_subdirectory(libs/emu)

# Add modular z-plane support libraries
add_subdirectory(libs/zplane_core)
add_subdirectory(libs/zplane_models)
add_subdirectory(libs/zplane_engine)

# Add modular pitchEngine DSP library
add_subdirectory(libs/pitchengine_dsp)


# MorphEngine plugin is defined below via juce_add_plugin (BUILD_MORPHENGINE option)

# Optional: chowdsp_utils modules (RT-safe utilities)
option(USE_CHOWDSP_UTILS "Use chowdsp_utils modules" ON)
set(CHOWDSP_UTILS_AVAILABLE OFF)
if (USE_CHOWDSP_UTILS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/modules/chowdsp_utils/modules")
    # Core/common utilities
    juce_add_module(${CMAKE_CURRENT_SOURCE_DIR}/modules/chowdsp_utils/modules/common/chowdsp_core)
    juce_add_module(${CMAKE_CURRENT_SOURCE_DIR}/modules/chowdsp_utils/modules/common/chowdsp_data_structures)
    juce_add_module(${CMAKE_CURRENT_SOURCE_DIR}/modules/chowdsp_utils/modules/common/chowdsp_json)
    juce_add_module(${CMAKE_CURRENT_SOURCE_DIR}/modules/chowdsp_utils/modules/common/chowdsp_listeners)
    juce_add_module(${CMAKE_CURRENT_SOURCE_DIR}/modules/chowdsp_utils/modules/common/chowdsp_logging)
    juce_add_module(${CMAKE_CURRENT_SOURCE_DIR}/modules/chowdsp_utils/modules/common/chowdsp_reflection)
    juce_add_module(${CMAKE_CURRENT_SOURCE_DIR}/modules/chowdsp_utils/modules/common/chowdsp_serialization)
    juce_add_module(${CMAKE_CURRENT_SOURCE_DIR}/modules/chowdsp_utils/modules/common/chowdsp_units)

    # DSP modules (select a reasonable subset)
    juce_add_module(${CMAKE_CURRENT_SOURCE_DIR}/modules/chowdsp_utils/modules/dsp/chowdsp_math)
    juce_add_module(${CMAKE_CURRENT_SOURCE_DIR}/modules/chowdsp_utils/modules/dsp/chowdsp_filters)
    juce_add_module(${CMAKE_CURRENT_SOURCE_DIR}/modules/chowdsp_utils/modules/dsp/chowdsp_buffers)
    juce_add_module(${CMAKE_CURRENT_SOURCE_DIR}/modules/chowdsp_utils/modules/dsp/chowdsp_dsp_utils)
    juce_add_module(${CMAKE_CURRENT_SOURCE_DIR}/modules/chowdsp_utils/modules/dsp/chowdsp_dsp_data_structures)
    juce_add_module(${CMAKE_CURRENT_SOURCE_DIR}/modules/chowdsp_utils/modules/dsp/chowdsp_simd)

    # GUI helpers (optional, lightweight)
    juce_add_module(${CMAKE_CURRENT_SOURCE_DIR}/modules/chowdsp_utils/modules/gui/chowdsp_gui)

    # Plugin utilities (params, state, base)
    juce_add_module(${CMAKE_CURRENT_SOURCE_DIR}/modules/chowdsp_utils/modules/plugin/chowdsp_parameters)
    juce_add_module(${CMAKE_CURRENT_SOURCE_DIR}/modules/chowdsp_utils/modules/plugin/chowdsp_plugin_base)
    juce_add_module(${CMAKE_CURRENT_SOURCE_DIR}/modules/chowdsp_utils/modules/plugin/chowdsp_plugin_utils)
    juce_add_module(${CMAKE_CURRENT_SOURCE_DIR}/modules/chowdsp_utils/modules/plugin/chowdsp_plugin_state)
    juce_add_module(${CMAKE_CURRENT_SOURCE_DIR}/modules/chowdsp_utils/modules/plugin/chowdsp_version)

    set(CHOWDSP_UTILS_AVAILABLE ON)
endif()

# EMU Extraction Tools
option(BUILD_TOOLS_X3 "Build EMU extraction tools in tools/x3" OFF)
if (BUILD_TOOLS_X3)
    add_subdirectory(tools/x3)
else ()
    # Make tools available but not built by default
    add_subdirectory(tools/x3 EXCLUDE_FROM_ALL)
endif ()

# FieldEngine configuration options
option(FIELDENGINE_ENABLE_SAMPLES "Enable loading of Resources/Samples at runtime" OFF)
set(FIELDENGINE_CLEAN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/fieldEngineClean" CACHE STRING "Path to reverse engineering data (not used at runtime)")

# Build switches: focus on morphEngine only for clean development
option(BUILD_FIELDENGINE_SYNTH "Build FieldEngineSynth plugin" OFF)
option(BUILD_FIELDENGINE_FX    "Build FieldEngineFX plugin"    OFF)
option(BUILD_EMU_TEST          "Build EMU Test plugin"         OFF)
option(BUILD_MRM               "Build MorphicRhythmMatrix plugin" OFF)
option(BUILD_PITCHENGINE       "Build pitchEnginePro plugin" OFF)
option(BUILD_SECRETSAUCE       "Build EnginesSecretSauce plugin" OFF)
option(BUILD_MORPHENGINE       "Build morphEngine plugin" ON)

# UI Framework options
option(USE_FOLEYS_UI           "Use Foleys GUI Magic for UI" OFF)
option(USE_FAUST_ZPLANE        "Use Faust for Z-plane processing" ON)

# See `docs/CMake API.md` in the JUCE repo for all config options
if (BUILD_FIELDENGINE_SYNTH)
    # FieldEngineSynth (instrument)
    juce_add_plugin(FieldEngineSynth
        ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/packaging/icon.png"
        COMPANY_NAME "${COMPANY_NAME}"
        BUNDLE_ID "com.pamplejuce.fieldenginesynth"
        COPY_PLUGIN_AFTER_BUILD TRUE
        PLUGIN_MANUFACTURER_CODE Pamp
        PLUGIN_CODE FES1
        FORMATS ${FORMATS}
        PRODUCT_NAME "FieldEngineSynth"
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    )
endif()

if (BUILD_FIELDENGINE_FX)
    # FieldEngineFX (effect)
    juce_add_plugin(FieldEngineFX
        ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/packaging/icon.png"
        COMPANY_NAME "${COMPANY_NAME}"
        BUNDLE_ID "com.pamplejuce.fieldenginefx"
        COPY_PLUGIN_AFTER_BUILD TRUE
        PLUGIN_MANUFACTURER_CODE Pamp
        PLUGIN_CODE FEFX
        FORMATS ${FORMATS}
        PRODUCT_NAME "FieldEngineFX"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    )
endif()

# This lets us use our code in both the JUCE targets and our Test target
# Without running into ODR violations
add_library(SharedCode INTERFACE)

# CLAP target intentionally not configured for multi-target setup

# Enable fast math, C++20 and a few other target defaults
include(SharedCodeDefaults)

# Shared sources (UI + DSP used by both plugins)
file(GLOB_RECURSE SharedSources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/source/shared/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/shared/*.h")

target_sources(SharedCode INTERFACE ${SharedSources})

# MorphicRhythmMatrix (third effect plugin)
if (BUILD_MRM)
    juce_add_plugin(MorphicRhythmMatrix
        ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/packaging/icon.png"
        COMPANY_NAME "${COMPANY_NAME}"
        BUNDLE_ID "com.pamplejuce.mrm"
        COPY_PLUGIN_AFTER_BUILD TRUE
        PLUGIN_MANUFACTURER_CODE Pamp
        PLUGIN_CODE MRM1
        FORMATS ${FORMATS}
        PRODUCT_NAME "MorphicRhythmMatrix"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    )
endif()

set(PITCHENGINEPRO_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/source/plugins/PitchEnginePro")

# pitchEnginePro (fourth effect plugin)
if (BUILD_PITCHENGINE AND EXISTS "${PITCHENGINEPRO_SOURCE_DIR}/PluginProcessor.cpp" AND EXISTS "${PITCHENGINEPRO_SOURCE_DIR}/PluginEditor.cpp")
    juce_add_plugin(pitchEnginePro
        ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/packaging/icon.png"
        COMPANY_NAME "${COMPANY_NAME}"
        BUNDLE_ID "com.yourcompany.pitchengine"
        COPY_PLUGIN_AFTER_BUILD TRUE
        PLUGIN_MANUFACTURER_CODE YCmp
        PLUGIN_CODE pEng
        FORMATS ${FORMATS}
        PRODUCT_NAME "pitchEngine Pro"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    )
elseif (BUILD_PITCHENGINE)
    message(WARNING "BUILD_PITCHENGINE is enabled but ${PITCHENGINEPRO_SOURCE_DIR} is missing. Skipping pitchEnginePro target.")
endif()

# EnginesSecretSauce (fifth effect plugin)
if (BUILD_SECRETSAUCE)
    juce_add_plugin(EnginesSecretSauce
        ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/packaging/icon.png"
        COMPANY_NAME "Engines"
        BUNDLE_ID "com.engines.secretsauce"
        COPY_PLUGIN_AFTER_BUILD TRUE
        PLUGIN_MANUFACTURER_CODE Engs
        PLUGIN_CODE SecS
        FORMATS ${FORMATS}
        PRODUCT_NAME "Engines SecretSauce"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    )
endif()

# morphEngine (clean EMU filter plugin)
if (BUILD_MORPHENGINE)
    juce_add_plugin(morphEngine
        ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/packaging/icon.png"
        COMPANY_NAME "${COMPANY_NAME}"
        BUNDLE_ID "com.pitchengine.morphengine"
        COPY_PLUGIN_AFTER_BUILD TRUE
        PLUGIN_MANUFACTURER_CODE YCmp
        PLUGIN_CODE mEng
        FORMATS ${FORMATS}
        PRODUCT_NAME "morphEngine"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    )

    # Fix VST2/VST3 conflict
    target_compile_definitions(morphEngine PUBLIC JUCE_VST3_CAN_REPLACE_VST2=0)
endif()

# Plugin-specific sources
if (BUILD_FIELDENGINE_SYNTH)
    # Synth
    set(FieldEngineSynth_Sources
        ${CMAKE_CURRENT_SOURCE_DIR}/source/synth/FieldEngineSynthProcessor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/source/synth/FieldEngineSynthProcessor.h
        ${CMAKE_CURRENT_SOURCE_DIR}/source/synth/FieldEngineSynthEditor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/source/synth/FieldEngineSynthEditor.h
    )
    target_sources(FieldEngineSynth PRIVATE ${FieldEngineSynth_Sources})
endif()

if (BUILD_FIELDENGINE_FX)
    # FX
    option(FX_ENABLE_VIRAL_UI "Enable Viral (experimental) UI for FX" OFF)
    set(FieldEngineFX_Sources
        ${CMAKE_CURRENT_SOURCE_DIR}/source/fx/FieldEngineFXProcessor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/source/fx/FieldEngineFXEditor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/source/dsp/MorphEngine.cpp
    )
    if (FX_ENABLE_VIRAL_UI)
        list(APPEND FieldEngineFX_Sources
            ${CMAKE_CURRENT_SOURCE_DIR}/source/ui/ViralEditor.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/source/ui/FELookAndFeel.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/source/ui/CartographyView.cpp
        )
    endif()
    target_sources(FieldEngineFX PRIVATE ${FieldEngineFX_Sources})
    if (USE_FAUST_ZPLANE)
        list(APPEND FieldEngineFX_Sources
            ${CMAKE_CURRENT_SOURCE_DIR}/source/faust/FaustZPlaneProcessor.cpp
        )
        target_include_directories(FieldEngineFX PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/source/faust
            ${CMAKE_CURRENT_SOURCE_DIR}/source/faust/gen
        )
    endif()

    if (USE_FOLEYS_UI)
        if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/modules/foleys_gui_magic/CMakeLists.txt)
            juce_add_module(${CMAKE_CURRENT_SOURCE_DIR}/modules/foleys_gui_magic/modules/foleys_gui_magic)
            target_link_libraries(FieldEngineFX PRIVATE foleys_gui_magic)
        endif()
        list(APPEND FieldEngineFX_Sources
            ${CMAKE_CURRENT_SOURCE_DIR}/source/foleys/FoleysFieldEngineProcessor.cpp
        )
    endif()
endif()

if (BUILD_EMU_TEST)
    # EMU Test (effect)
    juce_add_plugin(EMUTest
        ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/packaging/icon.png"
        COMPANY_NAME "${COMPANY_NAME}"
        BUNDLE_ID "com.pamplejuce.emutest"
        COPY_PLUGIN_AFTER_BUILD TRUE
        PLUGIN_MANUFACTURER_CODE Pamp
        PLUGIN_CODE EMUT
        FORMATS VST3
        PRODUCT_NAME "EMU Test"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    )
    
    set(EMUTest_Sources
        ${CMAKE_CURRENT_SOURCE_DIR}/source/test/EMUTestProcessor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/source/test/EMUTestProcessor.h
    )
    target_sources(EMUTest PRIVATE ${EMUTest_Sources})

    # Ensure VST2/VST3 param ID conflict guard is disabled (we do not ship VST2)
    target_compile_definitions(EMUTest PRIVATE
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_IGNORE_VST3_MISMATCHED_PARAMETER_ID_WARNING=1)
endif()

if (BUILD_MRM)
    # MRM
    set(MorphicRhythmMatrix_Sources
        ${CMAKE_CURRENT_SOURCE_DIR}/source/mrm/MorphicRhythmMatrixProcessor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/source/mrm/MorphicRhythmMatrixProcessor.h
        ${CMAKE_CURRENT_SOURCE_DIR}/source/ui/UiStateProvider.h
        ${CMAKE_CURRENT_SOURCE_DIR}/source/ui/TemplePalette.h
        ${CMAKE_CURRENT_SOURCE_DIR}/source/ui/TempleLookAndFeel.h
        ${CMAKE_CURRENT_SOURCE_DIR}/source/ui/TempleLookAndFeel.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/source/ui/TempleWidgets.h
        ${CMAKE_CURRENT_SOURCE_DIR}/source/ui/TempleEditor.h
        ${CMAKE_CURRENT_SOURCE_DIR}/source/ui/TempleEditor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/source/ui/SimpleEditor.cpp
    )
    target_sources(MorphicRhythmMatrix PRIVATE ${MorphicRhythmMatrix_Sources})
endif()

if (TARGET pitchEnginePro)
    set(PitchEnginePro_Sources
        ${PITCHENGINEPRO_SOURCE_DIR}/PluginProcessor.cpp
        ${PITCHENGINEPRO_SOURCE_DIR}/PluginEditor.cpp
        ${PITCHENGINEPRO_SOURCE_DIR}/dsp/Analyzer.cpp
        ${PITCHENGINEPRO_SOURCE_DIR}/dsp/PitchTracker.cpp
        ${PITCHENGINEPRO_SOURCE_DIR}/dsp/Shifter.cpp
        ${PITCHENGINEPRO_SOURCE_DIR}/dsp/Snapper.cpp
        ${PITCHENGINEPRO_SOURCE_DIR}/dsp/ZPlaneStyle.cpp
    )
    target_sources(pitchEnginePro PRIVATE ${PitchEnginePro_Sources})
    target_include_directories(pitchEnginePro PRIVATE ${PITCHENGINEPRO_SOURCE_DIR})

    # Remove dependency on pitchengine_dsp since we're compiling directly
endif()

if (BUILD_SECRETSAUCE)
    set(EnginesSecretSauce_Sources
        ${CMAKE_CURRENT_SOURCE_DIR}/enginesSecretSauce/Source/PluginProcessor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/enginesSecretSauce/Source/PluginEditor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/enginesSecretSauce/Source/SecretSauceEngine.cpp
    )
    target_sources(EnginesSecretSauce PRIVATE ${EnginesSecretSauce_Sources})
    target_include_directories(EnginesSecretSauce PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/enginesSecretSauce/Source)
endif()

if (BUILD_MORPHENGINE)
    set(MorphEngine_Sources
        ${CMAKE_CURRENT_SOURCE_DIR}/plugins/morphengine/src/MorphEngineAudioProcessor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/plugins/morphengine/src/PremiumMorphUI.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/plugins/morphengine/src/ResonanceLoom.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/plugins/morphengine/src/entry.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/AuthenticEMUZPlane.cpp
    )
    target_sources(morphEngine PRIVATE ${MorphEngine_Sources})
    target_include_directories(morphEngine PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/plugins/morphengine
        ${CMAKE_CURRENT_SOURCE_DIR}/plugins/morphengine/src
        ${CMAKE_CURRENT_SOURCE_DIR}/plugins/_shared
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/pitchengine_dsp/include
    )

    # Link to our modular DSP library
    target_link_libraries(morphEngine PRIVATE pitchengine_dsp)
    target_link_libraries(morphEngine PRIVATE SharedCode)

    # Add required modules for Standalone support
    target_link_libraries(morphEngine PRIVATE juce_audio_utils juce_audio_devices)
endif()


# Adds a BinaryData target for embedding assets into the binary
include(Assets)

if (TARGET zplane_engine)
    target_link_libraries(zplane_engine PUBLIC Assets)
endif()

# MacOS only: Cleans up folder and target organization on Xcode.
include(XcodePrettify)

# This is where you can set preprocessor definitions for JUCE and your plugin
target_compile_definitions(SharedCode
    INTERFACE

    # JUCE_WEB_BROWSER and JUCE_USE_CURL off by default
    JUCE_WEB_BROWSER=0  # If you set this to 1, add `NEEDS_WEB_BROWSER TRUE` to the `juce_add_plugin` call
    JUCE_USE_CURL=0     # If you set this to 1, add `NEEDS_CURL TRUE` to the `juce_add_plugin` call
    # Be conservative with Windows text/rendering backends to maximise host compatibility
    JUCE_USE_DIRECTWRITE=0
    JUCE_USE_HARFBUZZ=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_IGNORE_VST3_MISMATCHED_PARAMETER_ID_WARNING=1

    # Uncomment if you are paying for a an Indie/Pro license or releasing under GPLv3
    # JUCE_DISPLAY_SPLASH_SCREEN=0

    # lets the app known if we're Debug or Release
    CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
    VERSION="${CURRENT_VERSION}"

    # JucePlugin_Name is for some reason doesn't use the nicer PRODUCT_NAME
    PRODUCT_NAME_WITHOUT_VERSION="FieldEngine"
    FIELDENGINE_ENABLE_SAMPLES=$<IF:$<BOOL:${FIELDENGINE_ENABLE_SAMPLES}>,1,0>
    FIELDENGINE_CLEAN_DIR_PATH="${FIELDENGINE_CLEAN_DIR}"
)

# Link to any other modules you added (with juce_add_module) here!
# Usually JUCE modules must have PRIVATE visibility
# See https://github.com/juce-framework/JUCE/blob/master/docs/CMake%20API.md#juce_add_module
# However, with Pamplejuce, you'll link modules to SharedCode with INTERFACE visibility
# This allows the JUCE plugin targets and the Tests target to link against it
target_link_libraries(SharedCode
    INTERFACE
    Assets
    melatonin_inspector
    $<$<BOOL:${CHOWDSP_UTILS_AVAILABLE}>:chowdsp_core>
    $<$<BOOL:${CHOWDSP_UTILS_AVAILABLE}>:chowdsp_data_structures>
    $<$<BOOL:${CHOWDSP_UTILS_AVAILABLE}>:chowdsp_json>
    $<$<BOOL:${CHOWDSP_UTILS_AVAILABLE}>:chowdsp_listeners>
    $<$<BOOL:${CHOWDSP_UTILS_AVAILABLE}>:chowdsp_logging>
    $<$<BOOL:${CHOWDSP_UTILS_AVAILABLE}>:chowdsp_reflection>
    $<$<BOOL:${CHOWDSP_UTILS_AVAILABLE}>:chowdsp_serialization>
    $<$<BOOL:${CHOWDSP_UTILS_AVAILABLE}>:chowdsp_units>
    $<$<BOOL:${CHOWDSP_UTILS_AVAILABLE}>:chowdsp_math>
    $<$<BOOL:${CHOWDSP_UTILS_AVAILABLE}>:chowdsp_filters>
    $<$<BOOL:${CHOWDSP_UTILS_AVAILABLE}>:chowdsp_buffers>
    $<$<BOOL:${CHOWDSP_UTILS_AVAILABLE}>:chowdsp_dsp_utils>
    $<$<BOOL:${CHOWDSP_UTILS_AVAILABLE}>:chowdsp_dsp_data_structures>
    $<$<BOOL:${CHOWDSP_UTILS_AVAILABLE}>:chowdsp_simd>
    $<$<BOOL:${CHOWDSP_UTILS_AVAILABLE}>:chowdsp_gui>
    $<$<BOOL:${CHOWDSP_UTILS_AVAILABLE}>:chowdsp_parameters>
    $<$<BOOL:${CHOWDSP_UTILS_AVAILABLE}>:chowdsp_plugin_base>
    $<$<BOOL:${CHOWDSP_UTILS_AVAILABLE}>:chowdsp_plugin_utils>
    $<$<BOOL:${CHOWDSP_UTILS_AVAILABLE}>:chowdsp_plugin_state>
    $<$<BOOL:${CHOWDSP_UTILS_AVAILABLE}>:chowdsp_version>
    juce_audio_utils
    juce_audio_devices
    juce_audio_formats
    juce_audio_processors
    juce_dsp
    juce_gui_basics
    juce_gui_extra
    juce_opengl
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

# Link the JUCE plugin targets to our SharedCode target
if (BUILD_FIELDENGINE_SYNTH)
    target_link_libraries(FieldEngineSynth PRIVATE SharedCode)
endif()
if (BUILD_FIELDENGINE_FX)
    target_link_libraries(FieldEngineFX PRIVATE SharedCode)
endif()
if (BUILD_EMU_TEST)
    target_link_libraries(EMUTest PRIVATE emu_core)
endif()
if (BUILD_MRM)
    target_link_libraries(MorphicRhythmMatrix PRIVATE SharedCode)
endif()
if (TARGET pitchEnginePro)
    target_link_libraries(pitchEnginePro PRIVATE
        Assets
        juce_audio_utils
        juce_audio_devices
        juce_audio_formats
        juce_audio_processors
        juce_dsp
        juce_gui_basics
        juce_gui_extra
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
    )
    # Add compile definitions for pitchEnginePro
    target_compile_definitions(pitchEnginePro PUBLIC
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_IGNORE_VST3_MISMATCHED_PARAMETER_ID_WARNING=1
    )
endif()
if (BUILD_SECRETSAUCE)
    target_link_libraries(EnginesSecretSauce PRIVATE SharedCode)
endif()

if (BUILD_FIELDENGINE_SYNTH)
    juce_generate_juce_header(FieldEngineSynth)
endif()
if (BUILD_FIELDENGINE_FX)
    juce_generate_juce_header(FieldEngineFX)
endif()
if (BUILD_EMU_TEST)
    juce_generate_juce_header(EMUTest)
endif()
if (BUILD_MRM)
    juce_generate_juce_header(MorphicRhythmMatrix)
endif()
if (TARGET pitchEnginePro)
    juce_generate_juce_header(pitchEnginePro)
endif()
if (BUILD_SECRETSAUCE)
    juce_generate_juce_header(EnginesSecretSauce)
endif()

if (BUILD_MORPHENGINE)
    add_library(MorphEngineTestSupport STATIC
        ${CMAKE_CURRENT_SOURCE_DIR}/plugins/morphengine/src/MorphEngineAudioProcessor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/plugins/morphengine/src/TerminalMorphUI.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/AuthenticEMUZPlane.cpp
    )

    target_include_directories(MorphEngineTestSupport PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/plugins/morphengine
        ${CMAKE_CURRENT_SOURCE_DIR}/plugins/morphengine/src
        ${CMAKE_CURRENT_SOURCE_DIR}/plugins/_shared
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/pitchengine_dsp/include
    )

    target_link_libraries(MorphEngineTestSupport PUBLIC pitchengine_dsp SharedCode)
endif()

# --- QA test targets ---------------------------------------------------------
add_executable(test_basic_engine
    ${CMAKE_CURRENT_LIST_DIR}/qa/tests/test_basic_engine.cpp)
target_link_libraries(test_basic_engine PRIVATE emu_core juce_dsp)

add_executable(test_sr_invariance
    ${CMAKE_CURRENT_LIST_DIR}/qa/tests/test_sr_invariance.cpp)
target_link_libraries(test_sr_invariance PRIVATE emu_core juce_dsp)

add_executable(test_null_when_intensity_zero
    ${CMAKE_CURRENT_LIST_DIR}/qa/tests/test_null_when_intensity_zero.cpp)
target_link_libraries(test_null_when_intensity_zero PRIVATE emu_core juce_dsp)

if (BUILD_MORPHENGINE)
    add_executable(test_morphengine_latency
        ${CMAKE_CURRENT_LIST_DIR}/qa/tests/test_oversampling_latency.cpp)
    target_link_libraries(test_morphengine_latency PRIVATE MorphEngineTestSupport)

    add_executable(test_morphengine_morph_ramp
        ${CMAKE_CURRENT_LIST_DIR}/qa/tests/test_morph_ramp.cpp)
    target_link_libraries(test_morphengine_morph_ramp PRIVATE MorphEngineTestSupport)
endif()

add_executable(test_engine_denormals
    ${CMAKE_CURRENT_LIST_DIR}/qa/tests/test_denormals.cpp)
target_link_libraries(test_engine_denormals PRIVATE pitchengine_dsp juce_dsp)

add_executable(test_cpu_spike_silence
    ${CMAKE_CURRENT_LIST_DIR}/qa/tests/test_cpu_spike_silence.cpp)
target_link_libraries(test_cpu_spike_silence PRIVATE pitchengine_dsp juce_dsp)

add_executable(test_pole_stability
    ${CMAKE_CURRENT_LIST_DIR}/qa/tests/test_pole_stability.cpp)
target_link_libraries(test_pole_stability PRIVATE pitchengine_dsp juce_dsp)

add_executable(test_coefficient_continuity
    ${CMAKE_CURRENT_LIST_DIR}/qa/tests/test_coefficient_continuity.cpp)
target_link_libraries(test_coefficient_continuity PRIVATE pitchengine_dsp juce_dsp)

add_executable(test_passivity_scalar
    ${CMAKE_CURRENT_LIST_DIR}/qa/tests/test_passivity_scalar.cpp)
target_link_libraries(test_passivity_scalar PRIVATE zplane_engine)

add_executable(test_resonance_map
    ${CMAKE_CURRENT_LIST_DIR}/qa/tests/test_resonance_map.cpp)
target_link_libraries(test_resonance_map PRIVATE zplane_engine)
add_executable(test_wet_not_bypass
    ${CMAKE_CURRENT_LIST_DIR}/qa/tests/test_wet_not_bypass.cpp)
target_link_libraries(test_wet_not_bypass PRIVATE zplane_engine)

# (SecretFX transparency test removed after consolidation)

# IPP support, comment out to disable
include(PamplejuceIPP)

# Everything related to the tests target
include(Tests)

# A separate target for Benchmarks (keeps the Tests target fast)
include(Benchmarks)

# Output some config for CI (like our PRODUCT_NAME)
include(GitHubENV)




