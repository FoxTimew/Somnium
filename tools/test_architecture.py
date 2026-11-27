# Exercises forbidden includes as well as the two narrowly scoped integration exceptions.
# Fixtures ensure a new integration directory cannot hide leaks in other engine modules.
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

SCANNER = Path(__file__).with_name("check_architecture.py")

class Boundaries(unittest.TestCase):
    def check_source(self, path, source, accepted):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            file = root / "engine" / path
            file.parent.mkdir(parents=True)
            file.write_text(source, encoding="utf-8")
            result = subprocess.run([sys.executable, str(SCANNER), str(root)], capture_output=True, text=True)
            self.assertEqual(result.returncode == 0, accepted, result.stdout + result.stderr)

    def test_debug_ui_only_uses_facades(self):
        for dependency, accepted in [("core", True), ("scene", True), ("input", True),
                                     ("platform_imgui", True), ("gpu_imgui", True),
                                     ("platform", False), ("gpu", False)]:
            with self.subTest(dependency=dependency):
                self.check_source("debug_ui/src/overlay.cpp", f"#include <somnium/{dependency}/api.hpp>", accepted)

    def test_base_modules_reject_ui_and_public_native_types(self):
        for module in ["platform", "gpu"]:
            for source in ['#include <imgui.h>', '#if SOMNIUM_ENABLE_DEBUG_UI\n#endif', '#ifdef IMGUI_VERSION\n#endif']:
                with self.subTest(module=module, source=source):
                    self.check_source(f"{module}/src/base.cpp", source, False)
            self.check_source(f"{module}/include/api.hpp", "SDL_Window* window;", False)

    def test_adapters_remain_inside_their_boundaries(self):
        self.check_source("platform/integration/adapter.cpp", '#include <imgui.h>\nSDL_Event event;', True)
        self.check_source("gpu/integration/adapter.cpp", '#include <imgui.h>\n#include <SDL3/SDL_gpu.h>', True)
        self.check_source("gpu/integration/adapter.cpp", 'SDL_Event event;', False)
        self.check_source("debug_ui/integration/adapter.cpp", '#include <somnium/gpu/device.hpp>', False)
        self.check_source("platform/integration/include/api.hpp", 'SDL_Event event;', False)

    def test_runtime_keeps_renderer_private(self):
        for source in ['#include <somnium/renderer/camera.hpp>', 'renderer::Camera camera();']:
            self.check_source("runtime/include/gameplay.hpp", source, False)
        self.check_source("runtime/src/extraction.cpp", '#include <somnium/renderer/renderer.hpp>', True)

    def test_input_is_entirely_native_free(self):
        for source in ['SDL_Event event;', '#include <SDL3/SDL_keyboard.h>',
                       '#include <somnium/platform/platform.hpp>']:
            self.check_source("input/src/mapping.cpp", source, False)
        self.check_source("input/include/input.hpp", '#include <somnium/core/input_event.hpp>', True)

    def test_public_headers_cannot_duplicate_mutable_engine_state(self):
        for source in ["inline int state = 1;", "struct X { static int state; };",
                       "inline int& state = other;", "inline const int* state = nullptr;",
                       "inline int value() { static int counter = 0; return ++counter; }"]:
            self.check_source("core/include/state.hpp", source, False)
        for source in ["inline constexpr int kValue = 1;", "static const int kValue = 1;",
                       "struct X { static int value(); };", "inline int value() { return 1; }",
                       "// inline int state;\n/* static int state; */"]:
            self.check_source("core/include/state.hpp", source, True)

if __name__ == "__main__":
    unittest.main()
