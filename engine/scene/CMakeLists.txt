# Owns EnTT scene data, hierarchy and extensible JSON codecs without rendering types.
# CPU tests exercise persistence and transactions independently of platform/GPU.
add_library(somnium_scene STATIC src/world.cpp src/serialization.cpp src/builtin_codecs.cpp src/rotation.cpp)
add_library(somnium::scene ALIAS somnium_scene)
target_include_directories(somnium_scene PUBLIC include)
target_link_libraries(somnium_scene PUBLIC somnium::core somnium::assets EnTT::EnTT nlohmann_json::nlohmann_json)
somnium_configure_target(somnium_scene)
if(BUILD_TESTING)
    somnium_add_test(somnium_scene_tests tests/world_tests.cpp scene)
    target_compile_definitions(somnium_scene_tests PRIVATE
        SOMNIUM_ASSET_FIXTURES="${PROJECT_SOURCE_DIR}/engine/assets/tests/fixtures")
    add_executable(somnium_scene_structure_guard tests/structure_guard.cpp)
    target_link_libraries(somnium_scene_structure_guard PRIVATE somnium::scene)
    somnium_configure_target(somnium_scene_structure_guard)
    add_test(NAME somnium_scene_structure_guard COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_structure_guard.py"
        $<TARGET_FILE:somnium_scene_structure_guard> --assertions $<IF:$<CONFIG:Debug>,on,off>)
endif()
