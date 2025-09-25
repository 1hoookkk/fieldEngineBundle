# Helper to link pitchengine_dsp library
function(link_pitchengine_dsp target)
    # Check if pitchengine_dsp target already exists
    if(TARGET pitchengine_dsp)
        target_link_libraries(${target} PRIVATE pitchengine_dsp)
    else()
        # Fallback: add subdirectory if target doesn't exist
        add_subdirectory(${CMAKE_SOURCE_DIR}/libs/pitchengine_dsp)
        target_link_libraries(${target} PRIVATE pitchengine_dsp)
    endif()
    
    # Add include directories
    target_include_directories(${target} PRIVATE
        ${CMAKE_SOURCE_DIR}/libs/pitchengine_dsp/include
    )
endfunction()
