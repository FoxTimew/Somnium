# Builds the foundations, offline shaders and offscreen GPU proof.
# Dependencies are scoped to targets; dormant modules create no dummy libraries.
cmake_minimum_required(VERSION 3.28)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version; MSL 2.3 baseline")
project(Somnium VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
option(SOMNIUM_ENABLE_DEBUG_UI "Build the optional ImGui integrations" ON)
option(SOMNIUM_ENABLE_PROFILING "Enable Tracy CPU and memory instrumentation" OFF)
option(SOMNIUM_ENABLE_SANITIZERS "Enable address and undefined behavior sanitizers" OFF)
option(SOMNIUM_WARNINGS_AS_ERRORS "Treat Somnium warnings as errors" OFF)
option(SOMNIUM_VERIFY_SHADERCROSS_LIBRARY "Compile the legacy archive-only shadercross check" OFF)
include(cmake/Shaders.cmake)
include(CTest)
include(cmake/Warnings.cmake)
include(cmake/Sanitizers.cmake)
include(cmake/Dependencies.cmake)

if(MSVC)
    # CMake enables /EHsc by default. Remove it in our directory scope before
    # adding exception-free targets; dependency subdirectories retain their flags.
    string(REGEX REPLACE "(^| )[/-]EHsc( |$)" " " CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif()

add_library(somnium_build_options INTERFACE)
target_compile_features(somnium_build_options INTERFACE cxx_std_20)
target_compile_definitions(somnium_build_options INTERFACE
    GLM_FORCE_DEPTH_ZERO_TO_ONE GLM_FORCE_RADIANS GLM_FORCE_EXPLICIT_CTOR
    JSON_NOEXCEPTION SPDLOG_NO_EXCEPTIONS)
if(MINGW)
    target_link_options(somnium_build_options INTERFACE -static)
endif()

function(somnium_configure_target target)
    target_link_libraries(${target} PUBLIC somnium_build_options)
    somnium_warnings(${target})
    somnium_sanitizers(${target})
    if(MSVC)
        target_compile_options(${target} PRIVATE /GR- /EHs-c-)
        target_compile_definitions(${target} PRIVATE _HAS_EXCEPTIONS=0)
    else()
        target_compile_options(${target} PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions;-fno-rtti>)
    endif()
endfunction()

function(somnium_add_test target source module)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE somnium::${module} doctest::doctest)
    somnium_configure_target(${target})
    target_compile_definitions(${target} PRIVATE DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS)
    add_test(NAME ${target} COMMAND ${target})
endfunction()

foreach(module core platform gpu renderer assets scene input debug_ui runtime)
    add_subdirectory(engine/${module})
endforeach()
add_subdirectory(game)
add_subdirectory(sandbox)
add_subdirectory(tools)
include(cmake/Architecture.cmake)
