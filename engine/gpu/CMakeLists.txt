# Owns GPU resources; the offscreen executable is driven explicitly by D5, not skipped in CTest.
# CPU manifest contracts use doctest independently of device availability.
add_library(somnium_gpu STATIC src/device.cpp src/shader_data.cpp)
add_library(somnium::gpu ALIAS somnium_gpu)
target_include_directories(somnium_gpu PUBLIC include)
target_link_libraries(somnium_gpu PUBLIC somnium::core somnium::platform PRIVATE SDL3::SDL3 nlohmann_json::nlohmann_json)
somnium_configure_target(somnium_gpu)
if(BUILD_TESTING)
    somnium_add_test(somnium_gpu_tests tests/shader_tests.cpp gpu)
    target_include_directories(somnium_gpu_tests PRIVATE src)
    target_link_libraries(somnium_gpu_tests PRIVATE nlohmann_json::nlohmann_json)
    add_executable(somnium_gpu_offscreen tests/offscreen.cpp)
    target_link_libraries(somnium_gpu_offscreen PRIVATE somnium::gpu SDL3::SDL3 nlohmann_json::nlohmann_json)
    target_compile_definitions(somnium_gpu_offscreen PRIVATE SOMNIUM_GPU_DEBUG=$<IF:$<CONFIG:Debug>,1,0>)
    somnium_configure_target(somnium_gpu_offscreen)
    somnium_add_shaders(TARGET somnium_gpu_offscreen NAME d5Vertex
        SOURCE "${PROJECT_SOURCE_DIR}/assets/shaders/passthrough.vert.hlsl"
        STAGE vertex ENTRYPOINT vsMain INCLUDE_DIRECTORY "${PROJECT_SOURCE_DIR}/assets/shaders")
    somnium_add_shaders(TARGET somnium_gpu_offscreen NAME d5Fragment
        SOURCE "${PROJECT_SOURCE_DIR}/assets/shaders/textured.frag.hlsl"
        STAGE fragment ENTRYPOINT psMain INCLUDE_DIRECTORY "${PROJECT_SOURCE_DIR}/assets/shaders")
    add_test(NAME somnium_gpu_comparison_tests COMMAND "${Python3_EXECUTABLE}"
        "${PROJECT_SOURCE_DIR}/tools/gpu/test_compare.py")
endif()
