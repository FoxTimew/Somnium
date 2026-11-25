# Builds foundational contracts and diagnostics without platform dependencies.
# Only the logging implementation sees spdlog.
add_library(somnium_core STATIC src/assert.cpp src/log.cpp src/logical_path.cpp)
add_library(somnium::core ALIAS somnium_core)
target_include_directories(somnium_core PUBLIC include)
target_link_libraries(somnium_core PUBLIC tl::expected glm::glm PRIVATE spdlog::spdlog)
somnium_configure_target(somnium_core)
if(SOMNIUM_ENABLE_PROFILING)
    target_link_libraries(somnium_core PUBLIC Tracy::TracyClient)
    target_compile_definitions(somnium_core PUBLIC SOMNIUM_ENABLE_PROFILING=1)
endif()
if(BUILD_TESTING)
    somnium_add_test(somnium_core_tests tests/core_tests.cpp core)
    target_sources(somnium_core_tests PRIVATE tests/log_tests.cpp)
    add_test(NAME somnium_log_output
        COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/test_log.py"
            $<TARGET_FILE:somnium_core_tests>)
endif()
