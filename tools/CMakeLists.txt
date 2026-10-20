# Configures the tools consumer and its actual sources.
# No empty executable or dummy implementation is introduced.
add_library(somnium_shadercross STATIC "${shadercross_SOURCE_DIR}/src/SDL_shadercross.c")
target_include_directories(somnium_shadercross SYSTEM PUBLIC "${shadercross_SOURCE_DIR}/include")
target_compile_definitions(somnium_shadercross PRIVATE SDL_SHADERCROSS_DXC)
target_link_libraries(somnium_shadercross PRIVATE SDL3::SDL3 spirv-cross-c)
# Phase 0 compiles the full library; the DXC-linked offline CLI is a phase 1 tool.
# No HLSL compiler is linked into any engine module or sandbox.
if(BUILD_TESTING)
    add_executable(somnium_dependency_tests dependencies/dependency_tests.cpp)
    target_link_libraries(somnium_dependency_tests PRIVATE somnium::core EnTT::EnTT
        nlohmann_json::nlohmann_json somnium_cgltf somnium_stb doctest::doctest)
    somnium_configure_target(somnium_dependency_tests)
    add_test(NAME somnium_dependency_tests COMMAND somnium_dependency_tests)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(NAME somnium_architecture
        COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/check_architecture.py"
        "${PROJECT_SOURCE_DIR}")
endif()
