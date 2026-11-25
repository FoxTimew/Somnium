# Configures the tools consumer and its actual sources.
# No empty executable or dummy implementation is introduced.
if(SOMNIUM_VERIFY_SHADERCROSS_LIBRARY)
    add_library(somnium_shadercross STATIC "${shadercross_SOURCE_DIR}/src/SDL_shadercross.c")
    target_include_directories(somnium_shadercross SYSTEM PUBLIC "${shadercross_SOURCE_DIR}/include")
    target_compile_definitions(somnium_shadercross PRIVATE SDL_SHADERCROSS_DXC)
    target_link_libraries(somnium_shadercross PRIVATE SDL3::SDL3 spirv-cross-c)
endif()
# Phase 0 compiles the full library; the DXC-linked offline CLI is a phase 1 tool.
# No HLSL compiler is linked into any engine module or sandbox.
if(BUILD_TESTING)
    add_test(NAME somnium_shader_build_tests
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/test_build.py"
            --root "${PROJECT_SOURCE_DIR}" --work "${CMAKE_CURRENT_BINARY_DIR}/shader-build-tests"
            --compiler "${SOMNIUM_SHADERCROSS_EXECUTABLE}"
            --cmake "${CMAKE_COMMAND}" --ninja "${CMAKE_MAKE_PROGRAM}")
    set_tests_properties(somnium_shader_build_tests PROPERTIES TIMEOUT 120 LABELS "shaders")
    add_executable(somnium_dependency_tests dependencies/dependency_tests.cpp)
    target_link_libraries(somnium_dependency_tests PRIVATE somnium::core EnTT::EnTT
        nlohmann_json::nlohmann_json somnium_cgltf somnium_stb doctest::doctest_with_main)
    somnium_configure_target(somnium_dependency_tests)
    add_test(NAME somnium_dependency_tests COMMAND somnium_dependency_tests)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(NAME somnium_architecture
        COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/check_architecture.py"
        "${PROJECT_SOURCE_DIR}")
    add_test(NAME somnium_architecture_rules
        COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/test_architecture.py")
endif()
