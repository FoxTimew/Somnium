# Reserves the debug_ui module in the dependency graph.
# A library is created only when this phase supplies real sources.
if(SOMNIUM_ENABLE_DEBUG_UI)
    # These real upstream backends compile now but are initialized only in phase 1.
    add_library(somnium_platform_imgui STATIC "${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp")
    add_library(somnium_gpu_imgui STATIC "${imgui_SOURCE_DIR}/backends/imgui_impl_sdlgpu3.cpp")
    foreach(integration somnium_platform_imgui somnium_gpu_imgui)
        target_include_directories(${integration} SYSTEM PRIVATE "${imgui_SOURCE_DIR}/backends")
        target_link_libraries(${integration} PRIVATE somnium_imgui SDL3::SDL3)
        if(MSVC)
            target_compile_options(${integration} PRIVATE /GR- /EHs-c-)
        else()
            target_compile_options(${integration} PRIVATE -fno-exceptions -fno-rtti)
        endif()
    endforeach()
endif()
