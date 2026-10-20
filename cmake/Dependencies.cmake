# Locks all MVP library sources before their APIs enter project code.
# Header-only libraries and implementation libraries retain separate targets.
include("${CMAKE_CURRENT_LIST_DIR}/CPM.cmake")
CPMAddPackage(NAME SDL GITHUB_REPOSITORY libsdl-org/SDL GIT_TAG release-3.4.18
    SYSTEM YES OPTIONS "SDL_SHARED OFF" "SDL_STATIC ON" "SDL_TEST_LIBRARY OFF"
    "SDL_TESTS OFF" "SDL_EXAMPLES OFF" "SDL_INSTALL OFF")
if(CMAKE_C_COMPILER_ID STREQUAL "Clang" AND CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 23)
    # SDL's release-only diagnostic globals trigger this newly introduced warning.
    # Scope the exception to upstream SDL; Somnium retains its complete warning set.
    target_compile_options(SDL3-static PRIVATE -Wno-unused-but-set-global)
endif()
CPMAddPackage(NAME glm GITHUB_REPOSITORY g-truc/glm GIT_TAG 1.0.3
    SYSTEM YES OPTIONS "GLM_BUILD_TESTS OFF" "GLM_BUILD_LIBRARY OFF")
CPMAddPackage(NAME EnTT GITHUB_REPOSITORY skypjack/entt GIT_TAG v4.0.0
    SYSTEM YES OPTIONS "ENTT_BUILD_TESTING OFF")
CPMAddPackage(NAME json GITHUB_REPOSITORY nlohmann/json GIT_TAG v3.12.0
    SYSTEM YES OPTIONS "JSON_BuildTests OFF" "JSON_Install OFF" "JSON_SystemInclude ON")
CPMAddPackage(NAME spdlog GITHUB_REPOSITORY gabime/spdlog GIT_TAG v1.17.0
    SYSTEM YES OPTIONS "SPDLOG_BUILD_SHARED OFF" "SPDLOG_BUILD_EXAMPLE OFF"
    "SPDLOG_BUILD_TESTS OFF" "SPDLOG_NO_EXCEPTIONS ON" "SPDLOG_INSTALL OFF")
CPMAddPackage(NAME tracy GITHUB_REPOSITORY wolfpld/tracy GIT_TAG v0.14.1
    SYSTEM YES OPTIONS "TRACY_ENABLE ${SOMNIUM_ENABLE_PROFILING}" "TRACY_STATIC ON"
    "TRACY_ONLY_LOCALHOST ON" "TRACY_NO_BROADCAST ON" "TRACY_NO_SYSTEM_TRACING ON"
    "TRACY_NO_SAMPLING ON" "TRACY_NO_CALLSTACK ON" "TRACY_DEBUGINFOD OFF"
    "TRACY_LIBUNWIND_BACKTRACE OFF")
if(APPLE)
    # Tracy's pinned sources use sprintf; Apple marks that libc API deprecated.
    # Keep the waiver private to the upstream client, never Somnium consumers.
    target_compile_options(TracyClient PRIVATE -Wno-deprecated-declarations)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # Pinned Tracy host diagnostics ignore POSIX results and use sprintf buffers.
    # Isolate these upstream warnings; project targets keep -Wall/-Wextra/-Werror.
    target_compile_options(TracyClient PRIVATE -Wno-unused-result -Wno-format-overflow)
endif()
CPMAddPackage(NAME expected GITHUB_REPOSITORY TartanLlama/expected GIT_TAG v1.3.1
    SYSTEM YES OPTIONS "EXPECTED_BUILD_TESTS OFF" "EXPECTED_BUILD_PACKAGE OFF")
CPMAddPackage(NAME doctest GITHUB_REPOSITORY doctest/doctest GIT_TAG v2.5.3
    SYSTEM YES OPTIONS "DOCTEST_WITH_TESTS OFF" "DOCTEST_WITH_MAIN_IN_STATIC_LIB OFF")
CPMAddPackage(NAME cgltf GITHUB_REPOSITORY jkuhlmann/cgltf GIT_TAG v1.15 DOWNLOAD_ONLY YES)
CPMAddPackage(NAME stb GITHUB_REPOSITORY nothings/stb
    GIT_TAG a7d4c2175630e1ea5b6f8dd00ab96fed0b97b96d DOWNLOAD_ONLY YES)
CPMAddPackage(NAME imgui GITHUB_REPOSITORY ocornut/imgui GIT_TAG v1.93.0 DOWNLOAD_ONLY YES)
CPMAddPackage(NAME shadercross GITHUB_REPOSITORY libsdl-org/SDL_shadercross
    GIT_TAG 1ff05bec573988a98ef9e0260b4da44f512b8367 GIT_SUBMODULES "" DOWNLOAD_ONLY YES)
CPMAddPackage(NAME spirv_cross GITHUB_REPOSITORY KhronosGroup/SPIRV-Cross
    GIT_TAG vulkan-sdk-1.4.363.0 SYSTEM YES OPTIONS "SPIRV_CROSS_CLI OFF"
    "SPIRV_CROSS_ENABLE_TESTS OFF" "SPIRV_CROSS_SKIP_INSTALL ON"
    "SPIRV_CROSS_SHARED OFF" "SPIRV_CROSS_STATIC ON")

add_library(somnium_cgltf STATIC "${PROJECT_SOURCE_DIR}/tools/dependencies/cgltf_impl.c")
target_include_directories(somnium_cgltf SYSTEM PUBLIC "${cgltf_SOURCE_DIR}")
add_library(somnium_stb STATIC "${PROJECT_SOURCE_DIR}/tools/dependencies/stb_impl.c")
target_include_directories(somnium_stb SYSTEM PUBLIC "${stb_SOURCE_DIR}")
if(SOMNIUM_ENABLE_DEBUG_UI)
    add_library(somnium_imgui STATIC "${imgui_SOURCE_DIR}/imgui.cpp"
        "${imgui_SOURCE_DIR}/imgui_draw.cpp" "${imgui_SOURCE_DIR}/imgui_tables.cpp"
        "${imgui_SOURCE_DIR}/imgui_widgets.cpp")
    target_include_directories(somnium_imgui SYSTEM PUBLIC "${imgui_SOURCE_DIR}")
endif()
