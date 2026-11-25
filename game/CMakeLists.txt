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
    add_test(NAME somnium_sandbox_asset_sources COMMAND "${Python3_EXECUTABLE}"
        "${PROJECT_SOURCE_DIR}/tools/assets/generate_sandbox.py" --verify)
endif()
