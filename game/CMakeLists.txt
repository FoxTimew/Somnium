# Configures the game consumer and its actual sources.
# No empty executable or dummy implementation is introduced.
add_library(somnium_game STATIC src/sandbox_game.cpp src/orbit_camera.cpp src/collision.cpp src/codecs.cpp)
add_library(somnium::game ALIAS somnium_game)
target_include_directories(somnium_game PUBLIC include)
target_link_libraries(somnium_game PUBLIC somnium::runtime)
somnium_configure_target(somnium_game)
if(BUILD_TESTING)
    somnium_add_test(somnium_game_tests tests/orbit_camera_tests.cpp game)
    somnium_add_test(somnium_game_scene_tests tests/scene_codec_tests.cpp scene)
    somnium_add_test(somnium_gameplay_tests tests/gameplay_tests.cpp game)
    target_compile_definitions(somnium_gameplay_tests PRIVATE
        SOMNIUM_GAME_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/courtyard.json"
        SOMNIUM_GAME_MAPPING="${PROJECT_SOURCE_DIR}/assets/input/courtyard.json")
    if(SOMNIUM_ENABLE_ITERATION)
        target_include_directories(somnium_gameplay_tests PRIVATE "${PROJECT_SOURCE_DIR}/engine/runtime/src")
        somnium_add_test(somnium_editor_tests tests/editor_tests.cpp game)
        target_include_directories(somnium_editor_tests PRIVATE "${PROJECT_SOURCE_DIR}/engine/runtime/src")
        target_link_libraries(somnium_editor_tests PRIVATE somnium::platform)
        target_compile_definitions(somnium_editor_tests PRIVATE
            SOMNIUM_GAME_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/courtyard.json"
            SOMNIUM_GAME_MAPPING="${PROJECT_SOURCE_DIR}/assets/input/courtyard.json")
        add_executable(somnium_iteration_benchmark tests/iteration_benchmark.cpp)
        target_link_libraries(somnium_iteration_benchmark PRIVATE somnium::game somnium::platform)
        target_compile_definitions(somnium_iteration_benchmark PRIVATE SOMNIUM_BENCHMARK_BUILD="$<CONFIG>")
        somnium_configure_target(somnium_iteration_benchmark)
        add_test(NAME somnium_iteration_benchmark COMMAND somnium_iteration_benchmark
            "${PROJECT_SOURCE_DIR}/assets" "${CMAKE_BINARY_DIR}/iteration-benchmark.json")
    endif()
    add_test(NAME somnium_sandbox_asset_sources COMMAND "${Python3_EXECUTABLE}"
        "${PROJECT_SOURCE_DIR}/tools/assets/generate_sandbox.py" --verify)
endif()
