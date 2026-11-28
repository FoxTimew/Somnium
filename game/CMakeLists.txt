# Configures the game consumer and its actual sources.
# No empty executable or dummy implementation is introduced.
add_library(somnium_game STATIC src/sandbox_game.cpp src/orbit_camera.cpp src/collision.cpp src/codecs.cpp)
add_library(somnium::game ALIAS somnium_game)
target_include_directories(somnium_game PUBLIC include)
target_link_libraries(somnium_game PUBLIC somnium::runtime)
somnium_configure_target(somnium_game)
if(SOMNIUM_ENABLE_ITERATION)
    file(GLOB_RECURSE game_generation_files CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp")
    list(APPEND game_generation_files "${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${game_generation_files})
    list(SORT game_generation_files)
    set(game_generation_input "")
    foreach(path IN LISTS game_generation_files)
        file(READ "${path}" contents)
        string(REPLACE "\r\n" "\n" contents "${contents}")
        string(SHA256 hash "${contents}")
        string(APPEND game_generation_input "${hash};")
    endforeach()
    string(SHA256 game_generation "${game_generation_input}")
    add_library(somnium_game_module MODULE src/sandbox_game.cpp src/orbit_camera.cpp
        src/collision.cpp src/codecs.cpp src/module_entry.cpp)
    target_include_directories(somnium_game_module PRIVATE include "${CMAKE_BINARY_DIR}/generated")
    target_link_libraries(somnium_game_module PRIVATE somnium::runtime)
    target_compile_definitions(somnium_game_module PRIVATE SOMNIUM_GAME_GENERATION=g_${game_generation})
    set_target_properties(somnium_game_module PROPERTIES CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN YES WINDOWS_EXPORT_ALL_SYMBOLS OFF)
    somnium_configure_target(somnium_game_module)
    function(somnium_publish_game target name)
        set(symbols)
        if(MSVC)
            list(APPEND symbols --symbols "$<TARGET_PDB_FILE:${target}>")
        endif()
        add_custom_target(${target}_publication ALL
            COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/gameplay/publish.py"
                --binary "$<TARGET_FILE:${target}>" ${symbols}
                --output "${CMAKE_BINARY_DIR}/bin/modules/${name}.json"
            DEPENDS ${target} "${PROJECT_SOURCE_DIR}/tools/gameplay/publish.py" VERBATIM)
    endfunction()
    somnium_publish_game(somnium_game_module game)
    if(BUILD_TESTING)
        add_library(somnium_game_v2_objects OBJECT src/sandbox_game.cpp src/orbit_camera.cpp
            src/collision.cpp src/codecs.cpp)
        target_include_directories(somnium_game_v2_objects PRIVATE include)
        target_link_libraries(somnium_game_v2_objects PRIVATE somnium::runtime)
        target_compile_definitions(somnium_game_v2_objects PRIVATE
            SOMNIUM_GAME_GENERATION=g_${game_generation}_v2 SOMNIUM_GAME_LAYOUT_V2=1)
        set_target_properties(somnium_game_v2_objects PROPERTIES CXX_VISIBILITY_PRESET hidden
            VISIBILITY_INLINES_HIDDEN YES)
        somnium_configure_target(somnium_game_v2_objects)
        foreach(variant v2 bad_start bad_codec bad_asset)
            add_library(somnium_game_${variant} MODULE src/module_entry.cpp
                $<TARGET_OBJECTS:somnium_game_v2_objects>)
            target_include_directories(somnium_game_${variant} PRIVATE include "${CMAKE_BINARY_DIR}/generated")
            target_link_libraries(somnium_game_${variant} PRIVATE somnium::runtime)
            target_compile_definitions(somnium_game_${variant} PRIVATE
                SOMNIUM_GAME_GENERATION=g_${game_generation}_v2 SOMNIUM_GAME_LAYOUT_V2=1)
            set_target_properties(somnium_game_${variant} PROPERTIES CXX_VISIBILITY_PRESET hidden
                VISIBILITY_INLINES_HIDDEN YES WINDOWS_EXPORT_ALL_SYMBOLS OFF)
            somnium_configure_target(somnium_game_${variant})
            somnium_publish_game(somnium_game_${variant} ${variant})
        endforeach()
        target_compile_definitions(somnium_game_v2 PRIVATE SOMNIUM_MODULE_PROOF=1)
        target_compile_definitions(somnium_game_bad_start PRIVATE SOMNIUM_MODULE_PROOF=2)
        target_compile_definitions(somnium_game_bad_codec PRIVATE SOMNIUM_MODULE_PROOF=3)
        target_compile_definitions(somnium_game_bad_asset PRIVATE SOMNIUM_MODULE_PROOF=4)
        add_library(somnium_game_bad_abi MODULE src/module_entry.cpp
            $<TARGET_OBJECTS:somnium_game_v2_objects>)
        target_include_directories(somnium_game_bad_abi PRIVATE include "${CMAKE_BINARY_DIR}/generated")
        target_link_libraries(somnium_game_bad_abi PRIVATE somnium::runtime)
        target_compile_definitions(somnium_game_bad_abi PRIVATE SOMNIUM_PROOF_ABI_MISMATCH=1
            SOMNIUM_GAME_GENERATION=g_${game_generation}_v2 SOMNIUM_GAME_LAYOUT_V2=1)
        set_target_properties(somnium_game_bad_abi PROPERTIES CXX_VISIBILITY_PRESET hidden
            VISIBILITY_INLINES_HIDDEN YES WINDOWS_EXPORT_ALL_SYMBOLS OFF)
        somnium_configure_target(somnium_game_bad_abi)
        somnium_publish_game(somnium_game_bad_abi bad_abi)
        add_library(somnium_game_missing_symbol MODULE tests/missing_module_symbol.cpp)
        somnium_configure_target(somnium_game_missing_symbol)
        somnium_publish_game(somnium_game_missing_symbol missing_symbol)
        add_executable(somnium_game_module_tests tests/module_reload.cpp)
        target_link_libraries(somnium_game_module_tests PRIVATE somnium::runtime somnium::platform)
        somnium_configure_target(somnium_game_module_tests)
        add_dependencies(somnium_game_module_tests somnium_game_module_publication
            somnium_game_v2_publication somnium_game_bad_start_publication
            somnium_game_bad_codec_publication somnium_game_bad_asset_publication
            somnium_game_bad_abi_publication somnium_game_missing_symbol_publication)
        add_test(NAME somnium_game_module_tests COMMAND somnium_game_module_tests
            "${CMAKE_BINARY_DIR}/bin/modules" "${PROJECT_SOURCE_DIR}/assets"
            "${CMAKE_BINARY_DIR}/game-module-proof.json")
        set_tests_properties(somnium_game_module_tests PROPERTIES TIMEOUT 120)
    endif()
endif()
if(BUILD_TESTING)
    somnium_add_test(somnium_game_tests tests/orbit_camera_tests.cpp game)
    somnium_add_test(somnium_game_scene_tests tests/scene_codec_tests.cpp scene)
    somnium_add_test(somnium_gameplay_tests tests/gameplay_tests.cpp game)
    target_compile_definitions(somnium_gameplay_tests PRIVATE
        SOMNIUM_GAME_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/courtyard.json"
        SOMNIUM_GAME_MAPPING="${PROJECT_SOURCE_DIR}/assets/input/courtyard.json")
    if(SOMNIUM_ENABLE_ITERATION)
        target_include_directories(somnium_gameplay_tests PRIVATE "${PROJECT_SOURCE_DIR}/engine/runtime/src")
        somnium_add_test(somnium_editor_tests tests/editor_tests.cpp game)
        target_include_directories(somnium_editor_tests PRIVATE "${PROJECT_SOURCE_DIR}/engine/runtime/src")
        target_link_libraries(somnium_editor_tests PRIVATE somnium::platform)
        target_compile_definitions(somnium_editor_tests PRIVATE
            SOMNIUM_GAME_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/courtyard.json"
            SOMNIUM_GAME_MAPPING="${PROJECT_SOURCE_DIR}/assets/input/courtyard.json")
        add_executable(somnium_iteration_benchmark tests/iteration_benchmark.cpp)
        target_link_libraries(somnium_iteration_benchmark PRIVATE somnium::game somnium::platform)
        target_compile_definitions(somnium_iteration_benchmark PRIVATE SOMNIUM_BENCHMARK_BUILD="$<CONFIG>")
        somnium_configure_target(somnium_iteration_benchmark)
        add_test(NAME somnium_iteration_benchmark COMMAND somnium_iteration_benchmark
            "${PROJECT_SOURCE_DIR}/assets" "${CMAKE_BINARY_DIR}/iteration-benchmark.json")
    endif()
    add_test(NAME somnium_sandbox_asset_sources COMMAND "${Python3_EXECUTABLE}"
        "${PROJECT_SOURCE_DIR}/tools/assets/generate_sandbox.py" --verify)
endif()
