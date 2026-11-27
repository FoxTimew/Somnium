# Isolated experiment: compiles unchanged engine sources into one shared image.
# This project is never added to the engine build or installed with the sandbox.
cmake_minimum_required(VERSION 3.28)
project(SomniumReloadSpike LANGUAGES C CXX)
get_filename_component(SOMNIUM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
option(SPIKE_PROFILING "Exercise a single shared Tracy client" OFF)
option(SOMNIUM_ENABLE_SANITIZERS "Instrument host, shared engine and modules" OFF)
set(SOMNIUM_WARNINGS_AS_ERRORS ON)
include("${SOMNIUM_ROOT}/cmake/Warnings.cmake")
include("${SOMNIUM_ROOT}/cmake/Sanitizers.cmake")
include("${SOMNIUM_ROOT}/cmake/CPM.cmake")
# Pins are read from the real dependency declarations, rather than duplicated.
file(READ "${SOMNIUM_ROOT}/cmake/Dependencies.cmake" dependencies)
function(spike_package name)
    string(REGEX MATCH "NAME ${name} GITHUB_REPOSITORY ([^ \n\r]+) GIT_TAG ([^ \n\r]+)" pin "${dependencies}")
    if(NOT pin)
        message(FATAL_ERROR "Pinned dependency not found: ${name}")
    endif()
    CPMAddPackage(NAME ${name} GITHUB_REPOSITORY ${CMAKE_MATCH_1} GIT_TAG ${CMAKE_MATCH_2}
        SYSTEM YES OPTIONS ${ARGN})
endfunction()
spike_package(glm "GLM_BUILD_LIBRARY OFF" "GLM_BUILD_TESTS OFF")
spike_package(EnTT "ENTT_BUILD_TESTING OFF")
spike_package(json "JSON_BuildTests OFF" "JSON_Install OFF" "JSON_SystemInclude ON")
spike_package(expected "EXPECTED_BUILD_TESTS OFF" "EXPECTED_BUILD_PACKAGE OFF")
if(MSVC)
    string(REGEX REPLACE "(^| )[/-]EHsc( |$)" " " CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif()
spike_package(spdlog "SPDLOG_BUILD_SHARED OFF" "SPDLOG_BUILD_EXAMPLE OFF"
    "SPDLOG_BUILD_TESTS OFF" "SPDLOG_NO_EXCEPTIONS ON" "SPDLOG_INSTALL OFF")
spike_package(SDL "SDL_SHARED OFF" "SDL_STATIC ON" "SDL_TEST_LIBRARY OFF"
    "SDL_TESTS OFF" "SDL_EXAMPLES OFF" "SDL_INSTALL OFF" "SDL_WAYLAND OFF")
if(SPIKE_PROFILING)
    spike_package(tracy "TRACY_ENABLE ON" "TRACY_STATIC OFF" "TRACY_ONLY_LOCALHOST ON"
        "TRACY_NO_BROADCAST ON" "TRACY_NO_SYSTEM_TRACING ON" "TRACY_NO_SAMPLING ON"
        "TRACY_NO_CALLSTACK ON" "TRACY_DEBUGINFOD OFF" "TRACY_LIBUNWIND_BACKTRACE OFF")
endif()
function(spike_options target)
    somnium_warnings(${target})
    somnium_sanitizers(${target})
    if(MSVC)
        target_compile_options(${target} PRIVATE /GR- /EHs-c-)
        target_compile_definitions(${target} PRIVATE _HAS_EXCEPTIONS=0)
    else()
        target_compile_options(${target} PRIVATE -fno-exceptions -fno-rtti)
    endif()
endfunction()
add_library(spike_engine SHARED
    "${SOMNIUM_ROOT}/engine/core/src/assert.cpp"
    "${SOMNIUM_ROOT}/engine/core/src/log.cpp"
    "${SOMNIUM_ROOT}/engine/core/src/logical_path.cpp"
    "${SOMNIUM_ROOT}/engine/scene/src/world.cpp"
    "${SOMNIUM_ROOT}/engine/scene/src/serialization.cpp"
    "${SOMNIUM_ROOT}/engine/scene/src/builtin_codecs.cpp"
    "${SOMNIUM_ROOT}/engine/scene/src/rotation.cpp"
    "${SOMNIUM_ROOT}/engine/input/src/input.cpp"
    "${SOMNIUM_ROOT}/engine/input/src/configuration.cpp"
    "${SOMNIUM_ROOT}/engine/platform/src/input_replay.cpp")
set_target_properties(spike_engine PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
foreach(module core assets scene input platform runtime)
    target_include_directories(spike_engine PUBLIC "${SOMNIUM_ROOT}/engine/${module}/include")
endforeach()
target_link_libraries(spike_engine PUBLIC EnTT::EnTT glm::glm tl::expected nlohmann_json::nlohmann_json
    PRIVATE spdlog::spdlog)
target_compile_definitions(spike_engine PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE GLM_FORCE_RADIANS
    GLM_FORCE_EXPLICIT_CTOR JSON_NOEXCEPTION SPDLOG_NO_EXCEPTIONS)
if(SPIKE_PROFILING)
    target_compile_definitions(spike_engine PUBLIC SOMNIUM_ENABLE_PROFILING=1)
    target_link_libraries(spike_engine PUBLIC Tracy::TracyClient)
endif()
spike_options(spike_engine)
foreach(variant v1 v2 bad_start bad_abi)
    add_library(spike_${variant} MODULE module.cpp)
    target_link_libraries(spike_${variant} PRIVATE spike_engine)
    target_compile_definitions(spike_${variant} PRIVATE SPIKE_GENERATION=${variant})
    set_target_properties(spike_${variant} PROPERTIES CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN YES)
    if(NOT variant STREQUAL "v1")
        target_compile_definitions(spike_${variant} PRIVATE SPIKE_NEW_LAYOUT=1)
    endif()
    if(variant STREQUAL "bad_start")
        target_compile_definitions(spike_${variant} PRIVATE SPIKE_FAIL_START=1)
    elseif(variant STREQUAL "bad_abi")
        target_compile_definitions(spike_${variant} PRIVATE SPIKE_BAD_ABI=1)
    endif()
    spike_options(spike_${variant})
endforeach()
add_executable(spike_host host.cpp)
target_link_libraries(spike_host PRIVATE spike_engine SDL3::SDL3)
spike_options(spike_host)
enable_testing()
add_test(NAME hot_reload_contract COMMAND spike_host $<TARGET_FILE:spike_v1>
    $<TARGET_FILE:spike_v2> $<TARGET_FILE:spike_bad_start> $<TARGET_FILE:spike_bad_abi>
    "${CMAKE_CURRENT_SOURCE_DIR}/replay.json" "${CMAKE_BINARY_DIR}/shadow")
set_tests_properties(hot_reload_contract PROPERTIES TIMEOUT 60)
add_executable(spike_rotation_audit rotation_audit.cpp)
target_link_libraries(spike_rotation_audit PRIVATE spike_engine)
spike_options(spike_rotation_audit)
add_test(NAME rotation_boundary_audit COMMAND spike_rotation_audit)
set_tests_properties(rotation_boundary_audit PROPERTIES TIMEOUT 10)
