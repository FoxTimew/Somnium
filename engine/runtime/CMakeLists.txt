# Wires the active subsystems and owns the fixed-step loop.
# Later scene/render extraction is absent until its phase supplies real data.
add_library(somnium_runtime STATIC src/engine.cpp src/fixed_step.cpp src/model_rendering.cpp src/gameplay.cpp)
add_library(somnium::runtime ALIAS somnium_runtime)
target_include_directories(somnium_runtime PUBLIC include)
target_link_libraries(somnium_runtime PUBLIC somnium::core somnium::input somnium::renderer PRIVATE somnium::platform somnium::gpu somnium::assets)
target_compile_definitions(somnium_runtime PRIVATE SOMNIUM_GPU_DEBUG=$<IF:$<CONFIG:Debug>,1,0>)
somnium_configure_target(somnium_runtime)
if(SOMNIUM_ENABLE_DEBUG_UI)
    target_link_libraries(somnium_runtime PRIVATE somnium::debug_ui)
    target_compile_definitions(somnium_runtime PRIVATE SOMNIUM_ENABLE_DEBUG_UI=1)
endif()
if(BUILD_TESTING)
    somnium_add_test(somnium_runtime_tests tests/fixed_step_tests.cpp runtime)
    add_executable(somnium_renderer_offscreen tests/cube.cpp)
    target_include_directories(somnium_renderer_offscreen PRIVATE src)
    target_link_libraries(somnium_renderer_offscreen PRIVATE somnium::runtime somnium::assets
        somnium::renderer somnium::platform SDL3::SDL3 nlohmann_json::nlohmann_json)
    target_compile_definitions(somnium_renderer_offscreen PRIVATE SOMNIUM_GPU_DEBUG=$<IF:$<CONFIG:Debug>,1,0>)
    somnium_configure_target(somnium_renderer_offscreen)
    add_test(NAME somnium_cube_reference_tests COMMAND "${Python3_EXECUTABLE}"
        "${PROJECT_SOURCE_DIR}/tools/gpu/cube_reference.py" --verify)
endif()
