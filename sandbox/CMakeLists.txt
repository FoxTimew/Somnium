# Configures the sandbox consumer and its actual sources.
# No empty executable or dummy implementation is introduced.
add_executable(sandbox src/main.cpp)
target_link_libraries(sandbox PRIVATE somnium_game somnium::runtime)
somnium_configure_target(sandbox)
install(TARGETS sandbox RUNTIME DESTINATION bin)
if(BUILD_TESTING)
    if(SOMNIUM_DATA_MODE STREQUAL "COOKED")
        add_test(NAME somnium_window_smoke COMMAND "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/tools/test_package.py" --executable $<TARGET_FILE:sandbox>
            --model "${PROJECT_SOURCE_DIR}/assets/models/orbit-cube.glb"
            --scene "${PROJECT_SOURCE_DIR}/assets/scenes/orbit.json")
    else()
        add_test(NAME somnium_window_smoke COMMAND sandbox --frames 12)
    endif()
    set_tests_properties(somnium_window_smoke PROPERTIES TIMEOUT 30 LABELS "window"
        ENVIRONMENT "SDL_ASSERT=abort;SDL_LOGGING=*=verbose")
endif()
