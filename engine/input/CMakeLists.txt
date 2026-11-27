# Translates neutral events into fixed-step actions with no platform dependency.
# Tests exercise the same mapping entirely on the CPU.
add_library(somnium_input STATIC src/input.cpp src/configuration.cpp)
add_library(somnium::input ALIAS somnium_input)
target_include_directories(somnium_input PUBLIC include)
target_link_libraries(somnium_input PUBLIC somnium::core PRIVATE nlohmann_json::nlohmann_json)
somnium_configure_target(somnium_input)
if(BUILD_TESTING)
    somnium_add_test(somnium_input_tests tests/input_tests.cpp input)
endif()
