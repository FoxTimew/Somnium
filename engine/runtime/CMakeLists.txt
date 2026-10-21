# Wires the active subsystems and owns the fixed-step loop.
# Later scene/render extraction is absent until its phase supplies real data.
add_library(somnium_runtime STATIC src/engine.cpp src/fixed_step.cpp)
add_library(somnium::runtime ALIAS somnium_runtime)
target_include_directories(somnium_runtime PUBLIC include)
target_link_libraries(somnium_runtime PUBLIC somnium::core PRIVATE somnium::platform somnium::gpu)
target_compile_definitions(somnium_runtime PRIVATE SOMNIUM_GPU_DEBUG=$<IF:$<CONFIG:Debug>,1,0>)
somnium_configure_target(somnium_runtime)
if(BUILD_TESTING)
    somnium_add_test(somnium_runtime_tests tests/fixed_step_tests.cpp runtime)
endif()
