# Owns the SDL lifetime and byte access behind public Somnium types.
# Private SDL headers do not propagate to engine consumers.
add_library(somnium_platform STATIC src/platform.cpp src/file_reader.cpp)
add_library(somnium::platform ALIAS somnium_platform)
target_include_directories(somnium_platform PUBLIC include)
target_link_libraries(somnium_platform PUBLIC somnium::core PRIVATE SDL3::SDL3)
somnium_configure_target(somnium_platform)
if(BUILD_TESTING)
    somnium_add_test(somnium_platform_tests tests/file_reader_tests.cpp platform)
    target_compile_definitions(somnium_platform_tests PRIVATE
        SOMNIUM_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")
endif()
