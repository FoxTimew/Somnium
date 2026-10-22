# Owns the SDL lifetime and byte access behind public Somnium types.
# Private SDL headers do not propagate to engine consumers.
add_library(somnium_platform STATIC src/platform.cpp src/file_reader.cpp src/read_stream.cpp)
add_library(somnium::platform ALIAS somnium_platform)
target_include_directories(somnium_platform PUBLIC include PRIVATE private)
target_link_libraries(somnium_platform PUBLIC somnium::core PRIVATE SDL3::SDL3)
somnium_configure_target(somnium_platform)
if(SOMNIUM_DATA_MODE STREQUAL "DEVELOPMENT")
    target_compile_definitions(somnium_platform PRIVATE SOMNIUM_DEVELOPMENT_ASSETS="${PROJECT_SOURCE_DIR}/assets")
endif()
if(BUILD_TESTING)
    somnium_add_test(somnium_platform_tests tests/file_reader_tests.cpp platform)
    target_sources(somnium_platform_tests PRIVATE tests/read_stream_tests.cpp)
    target_include_directories(somnium_platform_tests PRIVATE src)
    target_link_libraries(somnium_platform_tests PRIVATE SDL3::SDL3)
    target_compile_definitions(somnium_platform_tests PRIVATE
        SOMNIUM_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")
    target_compile_definitions(somnium_platform_tests PRIVATE SOMNIUM_TEST_DEVELOPMENT_DATA=$<STREQUAL:${SOMNIUM_DATA_MODE},DEVELOPMENT>)
endif()
