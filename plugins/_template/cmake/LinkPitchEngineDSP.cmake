# Link the generated plugin target against libs/pitchengine_dsp
# This file is referenced by pamplejuce.yml:extra_cmake_files

if(NOT TARGET ${PROJECT_NAME})
  message(WARNING "LinkPitchEngineDSP.cmake included before target exists.")
endif()

# Option A: The top-level build already defines pitchengine_dsp (preferred)
if(TARGET pitchengine_dsp)
  target_link_libraries(${PROJECT_NAME} PRIVATE pitchengine_dsp)
else()
  # Option B: Add the library from the repo path without modifying legacy folders
  # Adjust the path if your repo layout differs.
  set(PITCHENGINE_DSP_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../libs/pitchengine_dsp")
  if(EXISTS "${PITCHENGINE_DSP_SOURCE_DIR}/CMakeLists.txt")
    add_subdirectory("${PITCHENGINE_DSP_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/_pitchengine_dsp")
    target_link_libraries(${PROJECT_NAME} PRIVATE pitchengine_dsp)
  else()
    message(WARNING "pitchengine_dsp not found at: ${PITCHENGINE_DSP_SOURCE_DIR}")
  endif()
endif()

