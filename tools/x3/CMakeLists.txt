cmake_minimum_required(VERSION 3.16)

project(x3_tools LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(test_e5p1_diagnostics
	tools/main.cpp
	x3_rom_scan.cpp
	x3_sample_extract.cpp
)

target_include_directories(test_e5p1_diagnostics PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if (MSVC)
	target_compile_options(test_e5p1_diagnostics PRIVATE /W4 /permissive-)
else()
	target_compile_options(test_e5p1_diagnostics PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(test_e5p1_diagnostics PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tools
)


