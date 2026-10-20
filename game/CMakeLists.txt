# Configures the game consumer and its actual sources.
# No empty executable or dummy implementation is introduced.
add_library(somnium_game STATIC src/foundation_game.cpp)
target_include_directories(somnium_game PUBLIC include)
target_link_libraries(somnium_game PUBLIC somnium::runtime)
somnium_configure_target(somnium_game)
