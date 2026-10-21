# Exercises the future consumer's fail-fast contract without touching engine CMake.
# A missing or unloadable compiler is a configuration error on every platform.
cmake_minimum_required(VERSION 3.28)
project(SomniumShaderToolchainConsumer NONE)
if(NOT EXISTS "${SOMNIUM_SHADERCROSS_EXECUTABLE}")
    message(FATAL_ERROR "SOMNIUM_SHADERCROSS_EXECUTABLE is required: install the pinned offline CLI")
endif()
execute_process(COMMAND "${SOMNIUM_SHADERCROSS_EXECUTABLE}" --help
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE error)
if(NOT status STREQUAL "0")
    message(FATAL_ERROR "Offline shadercross cannot run (${status}): ${output}${error}")
endif()
