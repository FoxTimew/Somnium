# Configures the game consumer and its actual sources.
# No empty executable or dummy implementation is introduced.
add_library(somnium_game STATIC src/foundation_game.cpp src/orbit_camera.cpp)
add_library(somnium::game ALIAS somnium_game)
target_include_directories(somnium_game PUBLIC include)
target_link_libraries(somnium_game PUBLIC somnium::runtime)
somnium_configure_target(somnium_game)
if(BUILD_TESTING)
    somnium_add_test(somnium_game_tests tests/orbit_camera_tests.cpp game)
    somnium_add_test(somnium_game_scene_tests tests/scene_codec_tests.cpp scene)
endif()
