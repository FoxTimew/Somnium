# Imports immutable CPU data through injected readers, with private pinned decoders.
# The module has no platform or GPU dependency.
add_library(somnium_assets STATIC src/image_loader.cpp src/model_loader.cpp)
add_library(somnium::assets ALIAS somnium_assets)
target_include_directories(somnium_assets PUBLIC include)
target_link_libraries(somnium_assets PUBLIC somnium::core PRIVATE somnium_cgltf somnium_stb)
somnium_configure_target(somnium_assets)
if(BUILD_TESTING)
    somnium_add_test(somnium_assets_tests tests/asset_tests.cpp assets)
    target_link_libraries(somnium_assets_tests PRIVATE nlohmann_json::nlohmann_json)
    target_compile_definitions(somnium_assets_tests PRIVATE SOMNIUM_ASSET_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
    add_test(NAME somnium_assets_warning COMMAND "${Python3_EXECUTABLE}"
        "${PROJECT_SOURCE_DIR}/tools/assets/test_warning.py" $<TARGET_FILE:somnium_assets_tests>)
endif()
