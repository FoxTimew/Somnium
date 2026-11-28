# Renders neutral draw lists; imported asset data stays behind the runtime adapter.
# Camera contracts are testable without a graphics device.
add_library(somnium_renderer STATIC src/renderer.cpp src/camera.cpp)
add_library(somnium::renderer ALIAS somnium_renderer)
target_include_directories(somnium_renderer PUBLIC include)
target_link_libraries(somnium_renderer PUBLIC somnium::core somnium::gpu)
somnium_configure_target(somnium_renderer)
somnium_add_shaders(TARGET somnium_renderer NAME modelVertex
    SOURCE "${PROJECT_SOURCE_DIR}/assets/shaders/model.vert.hlsl"
    STAGE vertex ENTRYPOINT vsMain INCLUDE_DIRECTORY "${PROJECT_SOURCE_DIR}/assets/shaders")
somnium_add_shaders(TARGET somnium_renderer NAME modelFragment
    SOURCE "${PROJECT_SOURCE_DIR}/assets/shaders/model.frag.hlsl"
    STAGE fragment ENTRYPOINT psMain INCLUDE_DIRECTORY "${PROJECT_SOURCE_DIR}/assets/shaders")
somnium_add_shaders(TARGET somnium_renderer NAME modelAmbient
    SOURCE "${PROJECT_SOURCE_DIR}/assets/shaders/model_ambient.frag.hlsl"
    STAGE fragment ENTRYPOINT psMain INCLUDE_DIRECTORY "${PROJECT_SOURCE_DIR}/assets/shaders")
if(SOMNIUM_ENABLE_ITERATION)
    target_sources(somnium_renderer PRIVATE src/shader_reload.cpp)
    target_link_libraries(somnium_renderer PRIVATE nlohmann_json::nlohmann_json)
    somnium_add_shader_program(TARGET somnium_renderer NAME model
        STAGES vertex=modelVertex lambert=modelFragment ambient=modelAmbient)
endif()
if(BUILD_TESTING)
    somnium_add_test(somnium_renderer_tests tests/camera_tests.cpp renderer)
    if(SOMNIUM_ENABLE_ITERATION)
        target_sources(somnium_renderer_tests PRIVATE tests/shader_reload_tests.cpp)
        target_include_directories(somnium_renderer_tests PRIVATE src)
        target_link_libraries(somnium_renderer_tests PRIVATE nlohmann_json::nlohmann_json)
        somnium_add_shaders(TARGET somnium_renderer NAME reloadRed
            SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/tests/shaders/reload_red.frag.hlsl"
            STAGE fragment ENTRYPOINT psMain INCLUDE_DIRECTORY "${PROJECT_SOURCE_DIR}/assets/shaders")
        somnium_add_shaders(TARGET somnium_renderer NAME reloadRedAmbient
            SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/tests/shaders/reload_red.frag.hlsl" DEFINES RELOAD_AMBIENT=1
            STAGE fragment ENTRYPOINT psMain INCLUDE_DIRECTORY "${PROJECT_SOURCE_DIR}/assets/shaders")
        somnium_add_shaders(TARGET somnium_renderer NAME reloadLayout
            SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/tests/shaders/reload_layout.frag.hlsl"
            STAGE fragment ENTRYPOINT psMain INCLUDE_DIRECTORY "${PROJECT_SOURCE_DIR}/assets/shaders")
    endif()
endif()
