# Exercises shipping guards against injected targets, sources and enabled features.
# The production check additionally reads the real generated CMake/Ninja graph in every CI run.
import unittest
from check_shipping import validate


class ShippingTests(unittest.TestCase):
    cache = "SOMNIUM_DATA_MODE:STRING=COOKED\nSOMNIUM_ENABLE_DEBUG_UI:BOOL=OFF\n"

    def test_static_game_and_existing_build_tools_are_allowed(self):
        validate("somnium_game: phony\nsandbox_lambert_vert_shaders: phony",
                 [{"file": "game/src/sandbox_game.cpp", "command": "c++ -c game.cpp"}], self.cache)
        validate("somnium_scene: phony", [{"file": "engine/scene/src/serialization.cpp"}], self.cache)

    def test_each_optional_target_is_rejected(self):
        for target in ["somnium_imgui", "somnium_platform_imgui", "somnium_gpu_imgui",
                       "somnium_debug_ui", "somnium_iteration", "somnium_editor_camera",
                       "somnium_file_watch", "somnium_shader_reload", "somnium_game_module",
                       "somnium_module_loader", "somnium_engine_shared", "spike_v1"]:
            with self.subTest(target=target), self.assertRaisesRegex(ValueError, "target"):
                validate(target + ": phony", [], self.cache)

    def test_sources_are_rejected_on_both_path_styles(self):
        for source in ["engine/platform/src/file_watch.cpp", "engine/platform/src/module_loader.cpp",
                       "engine/renderer/src/shader_reload.cpp", "engine/runtime/src/edit_session.cpp",
                       "engine/runtime/src/iteration_session.cpp", "engine/platform/src/source_files.cpp",
                       "engine/platform/src/source_files_atomic.cpp", "engine/runtime/src/scene_editor.cpp",
                       "game/tests/iteration_benchmark.cpp",
                       "engine/debug_ui/src/editor_camera.cpp", "tools/hot_reload_spike/module.cpp"]:
            for path in ["/repo/" + source, "C:\\repo\\" + source.replace("/", "\\")]:
                with self.subTest(path=path), self.assertRaisesRegex(ValueError, "source"):
                    validate("", [{"file": path}], self.cache)

    def test_enabled_options_are_rejected(self):
        for feature in ["ITERATION", "SHADER_RELOAD", "GAMEPLAY_RELOAD"]:
            for value in ["ON", "TRUE", "1", "YES"]:
                with self.subTest(feature=feature, value=value), self.assertRaisesRegex(ValueError, "enables"):
                    validate("", [], self.cache + f"SOMNIUM_ENABLE_{feature}:BOOL={value}\n")

    def test_definitions_cannot_hide_in_an_otherwise_allowed_source(self):
        for definition in ["/DSOMNIUM_ENABLE_ITERATION", "-DSOMNIUM_ENABLE_GAMEPLAY_RELOAD=1",
                           "-DSOMNIUM_ENABLE_SHADER_RELOAD=0"]:
            for flags in [{"command": "c++ " + definition}, {"arguments": ["c++", definition]}]:
                with self.subTest(flags=flags), self.assertRaisesRegex(ValueError, "definition"):
                    validate("", [{"file": "engine/runtime/src/engine.cpp", **flags}], self.cache)

    def test_development_root_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "cooked"):
            validate("", [], "SOMNIUM_DATA_MODE:STRING=DEVELOPMENT\n")
        with self.assertRaisesRegex(ValueError, "source root"):
            validate("", [{"file": "sandbox/main.cpp", "command": "c++ -DSOMNIUM_DEVELOPMENT_ASSETS=x"}], self.cache)


if __name__ == "__main__":
    unittest.main()
