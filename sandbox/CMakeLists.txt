# Configures the sandbox consumer and its actual sources.
# No empty executable or dummy implementation is introduced.
add_executable(sandbox src/main.cpp)
target_link_libraries(sandbox PRIVATE somnium_game somnium::runtime)
somnium_configure_target(sandbox)
somnium_add_shaders(TARGET sandbox NAME passthrough
    SOURCE "${PROJECT_SOURCE_DIR}/assets/shaders/passthrough.vert.hlsl"
    STAGE vertex ENTRYPOINT vsMain INCLUDE_DIRECTORY "${PROJECT_SOURCE_DIR}/assets/shaders")
somnium_add_shaders(TARGET sandbox NAME textured
    SOURCE "${PROJECT_SOURCE_DIR}/assets/shaders/textured.frag.hlsl"
    STAGE fragment ENTRYPOINT psMain INCLUDE_DIRECTORY "${PROJECT_SOURCE_DIR}/assets/shaders")
install(TARGETS sandbox RUNTIME DESTINATION bin)
if(BUILD_TESTING)
    add_test(NAME somnium_window_smoke COMMAND sandbox --frames 12)
    set_tests_properties(somnium_window_smoke PROPERTIES TIMEOUT 30 LABELS "window"
        ENVIRONMENT "SDL_ASSERT=abort;SDL_LOGGING=*=verbose")
endif()
