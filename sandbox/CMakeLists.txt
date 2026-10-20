# Configures the sandbox consumer and its actual sources.
# No empty executable or dummy implementation is introduced.
add_executable(sandbox src/main.cpp)
target_link_libraries(sandbox PRIVATE somnium_game somnium::runtime)
somnium_configure_target(sandbox)
if(BUILD_TESTING)
    add_test(NAME somnium_window_smoke COMMAND sandbox --frames 12)
    set_tests_properties(somnium_window_smoke PROPERTIES TIMEOUT 30 LABELS "window")
endif()
